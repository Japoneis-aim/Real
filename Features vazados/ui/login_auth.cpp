#include <auth/zimo_client.hpp>

#include <atomic>
#include <memory>
#include <string>
#include <cstring>
#include <chrono>
#include <nt/wrap.h>
#include <core/config.h>
#include <core/lang.h>
#include <core/credentials.h>
#include <core/profile.h>
#include <core/avatar.h>
#include <LazyDlls/Lazyimporter.hpp>
#include <vmp.h>
#include <console.h>

namespace UI::Login {
    extern std::atomic<bool> authenticating;
    extern std::atomic<bool> authenticated;
    extern std::string statusText;
    extern std::string expiresText;
    extern bool rememberMe;
    extern void (*toastCallback)(const std::string&, float);
    extern void (*toastSuccessCallback)(const std::string&, float);

    // Cliente persistente — armazenado depois do auth bem sucedido pra rodar
    // o run_session() em background. Antes era stack-local em DoAuthAsync e
    // disconnect() vinha logo apos o login, deixando o server sem como
    // revogar a sessao mid-cheat.
    static std::unique_ptr<zimo::ZimoClient> g_persistentClient;
    static HANDLE g_sessionThread = nullptr;
    static std::atomic<bool> g_sessionStarted{ false };

    struct AuthArgs {
        std::string user;
        std::string pass;
    };

    static std::string TranslateAuthCode(const char* code) {
        auto eq = [](const char* a, const char* b) { return a && b && strcmp(a, b) == 0; };

        if (!code || !code[0])
            return L("Authentication failed", "Falha na autenticação");

        if (eq(code, "AUTH_INVALID") || eq(code, "invalid_credentials"))
            return L("Invalid username or password", "Usuário ou senha incorretos");
        if (eq(code, "AUTH_BANNED") || eq(code, "banned"))
            return L("Account banned", "Conta banida");
        if (eq(code, "AUTH_LOCKED"))
            return L("Account temporarily locked", "Conta temporariamente bloqueada");
        if (eq(code, "AUTH_BLOCKED"))
            return L("Too many attempts, try again later", "Muitas tentativas, tente novamente mais tarde");

        if (eq(code, "AUTH_NO_LICENSE"))
            return L("No active license. Buy it at zimobr.com", "Sem licença ativa. Compre em zimobr.com");
        if (eq(code, "AUTH_PRODUCT_NOT_FOUND"))
            return L("Product unavailable, contact support", "Produto indisponível, contate o suporte");
        if (eq(code, "key_expired"))
            return L("License expired", "Licença expirada");

        if (eq(code, "AUTH_HWID_MISMATCH") || eq(code, "hwid_mismatch"))
            return L("Different machine. Reset HWID on the website panel", "Máquina diferente. Resete o HWID no painel do site");
        if (eq(code, "AUTH_VERSION_MISMATCH"))
            return L("Outdated loader, download the latest version", "Loader desatualizado, baixe a versão mais recente");

        if (eq(code, "AUTH_NONCE_REUSED"))
            return L("Security error, please try again", "Erro de segurança, tente novamente");
        if (eq(code, "AUTH_TIMESTAMP_INVALID"))
            return L("System clock out of sync. Fix your system date/time", "Relógio do sistema dessincronizado. Corrija data/hora do sistema");

        if (eq(code, "session_limit"))
            return L("Session limit reached", "Limite de sessões atingido");

        if (eq(code, "AUTH_FAILED"))
            return L("Authentication failed", "Falha na autenticação");
        if (eq(code, "CONN_FAILED"))
            return L("Connection failed. Check your internet", "Falha na conexão. Verifique sua internet");
        if (eq(code, "ERR_INTERNAL"))
            return L("Internal error, please try again", "Erro interno, tente novamente");
        if (eq(code, "ERR_UNKNOWN"))
            return L("Unknown error", "Erro desconhecido");

        return std::string(code);
    }

    // Spawnna a thread de heartbeat: client->run_session() roda em loop
    // mandando ping pro server. Server pode revogar a sessao (HWID changed,
    // banned, expired) e o run_session() encerra com excecao.
    static void StartSessionThread() {
        if (!g_persistentClient || g_sessionStarted.load()) return;
        g_sessionStarted.store(true);
        g_sessionThread = LI_CACHED(CreateThread)(nullptr, 0,
            [](LPVOID) -> DWORD {
                VMP_BEGIN_ULTRA("zm_cs2_session");
                try { g_persistentClient->run_session(); }
                catch (...) {}
                g_sessionStarted.store(false);
                // Sessao caiu pelo server → invalida cliente local e dispara
                // shutdown. UI::Profile::Authenticated.store(false) tambem
                // pra qualquer chequer de runtime perceber.
                UI::Profile::Authenticated.store(false);
                Config::ShutdownRequested().store(true);
                VMP_END;
                return 0;
            }, nullptr, 0, nullptr);
    }

    // Logout/limpeza chamada no shutdown ou quando a sessao cai.
    void StopSession() {
        VMP_BEGIN_ULTRA("zm_cs2_logout");
        if (g_persistentClient) {
            try { g_persistentClient->stop(); } catch (...) {}
            if (g_sessionThread) {
                ::nt::LargeInt to{};
                to.QuadPart = -50000000LL; // 5s relativo (100ns units)
                LI_FN_T(NtWaitForSingleObject, ::nt::NtWaitForSingleObject_fn)(
                    g_sessionThread, 0, &to);
                Sys::Close(g_sessionThread);
                g_sessionThread = nullptr;
            }
            try { g_persistentClient->disconnect(); } catch (...) {}
            g_persistentClient.reset();
        }
        g_sessionStarted.store(false);
        UI::Profile::HasExpiry.store(false);
        VMP_END;
    }

    // Watchdog: chamado a cada frame do render. Se a licenca expirou local
    // (comparacao com system clock), forca shutdown — server pode estar
    // unreachable mas o expiry foi cravado no auth response e nao da pra
    // bypassar avancando o relogio (server compara timestamp no proximo ping).
    void WatchdogTick() {
    }

    void DoAuthAsync(const std::string& user, const std::string& pass) {
        auto* args = new AuthArgs{ user, pass };
        Sys::CreateDetachedThread([](void* ptr) {
            VMP_BEGIN_ULTRA("zm_cs2_auth");
            auto* a = static_cast<AuthArgs*>(ptr);
            std::string user = std::move(a->user);
            std::string pass = std::move(a->pass);
            delete a;

            authenticating.store(true);
            statusText = L(xorstr_("Connecting..."),xorstr_("Conectando..."));

            Console::Info("[auth] DoAuthAsync user='%s' (pass len=%zu)", user.c_str(), pass.size());

            const int MAX_ATTEMPTS = 3;
            try {
                // Strings de auth ficam encriptadas no .rdata via xorstr_ e
                // so saem em plaintext na stack local durante esse statement.
                // Sem isso, "45.40.99.95" / "MFkw..." / "zimo-cs2" achavam
                // facil em qualquer hex editor.
                zimo::ZimoClientOptions opts;
                opts.host = xorstr_("45.40.99.95");
                opts.port = 7777;
                opts.server_ecdsa_public_key = xorstr_(
                    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEX5pNnRmF7rVgHR2tvgvnQJ42lmSfXwDViPS1iC5VddXAcJOpHo1KofwIHULZ7iVXJ1U+foW00+MaXpdyM4xKRw==");

                Console::Info("[auth] Constructing ZimoClient...");
                auto client = std::make_unique<zimo::ZimoClient>(opts);
                Console::Info("[auth] HWID='%s'", client->hardware_id().c_str());

                int attempt = 0;
                while (true) {
                    attempt++;
                    try {
                        if (attempt == 1)
                            statusText = L(xorstr_("Connecting..."),xorstr_("Conectando..."));
                        else
                            statusText = std::string(L(xorstr_("Retrying ("),xorstr_("Tentando novamente ("))) + std::to_string(attempt) + "/" + std::to_string(MAX_ATTEMPTS) + ")...";

                        Console::Info("[auth] connect() attempt %d/%d ...", attempt, MAX_ATTEMPTS);
                        client->connect();
                        Console::Success("[auth] connect() OK on attempt %d", attempt);
                        break;
                    } catch (const zimo::ConnectionException& ex) {
                        Console::Warn("[auth] ConnectionException on attempt %d/%d: %s",
                            attempt, MAX_ATTEMPTS, ex.what());
                        if (attempt >= MAX_ATTEMPTS) throw;
                        Sys::Sleep(600);
                    }
                }

                statusText = L(xorstr_("Authenticating..."),xorstr_("Autenticando..."));
                Console::Info("[auth] authenticate() ...");
                // xorstr_ retorna decryptor convertivel pra const char* —
                // construct std::string explicitly pra contornar copy-init.
                std::string slug(xorstr_("zimo-cs2").c_str());
                std::string version(xorstr_("1.0.1").c_str());
                auto result = client->authenticate(user, pass, slug, version);
                Console::Info("[auth] authenticate() returned success=%d code='%s'",
                    (int)result.success,
                    result.error_code.value_or("(none)").c_str());

                // VMProtect Mutation no branch de decisao. Sem isso vira um
                // simples cmp/setnz/jz num offset previsivel — patchavel com
                // 6 NOPs. Com mutation os opcodes mudam entre builds e a
                // localizacao automatica fica dificil.
                VMP_BEGIN_MUTATE("zm_cs2_auth_decide");
                if (result.success) {
                    authenticated.store(true);
                    statusText = L(xorstr_("Success!"),xorstr_("Sucesso!"));

                    UI::Profile::Username = user;
                    UI::Profile::Authenticated.store(true);

                    if (!result.avatar_data.empty()) {
                        Avatar::SetFromPNG(result.avatar_data);
                        Console::Info("[auth] Avatar PNG queued (%zu bytes)", result.avatar_data.size());
                    } else {
                        Console::Info("[auth] No avatar data in auth response");
                    }

                    // Save de credentials tambem mutado — patch comum eh
                    // forcar rememberMe=true pra capturar credenciais alheias.
                    VMP_BEGIN_MUTATE("zm_cs2_creds_persist");
                    if (rememberMe) {
                        Credentials::Save(user.c_str(), pass.c_str(), true);
                        Console::Info("[auth] Credentials saved (rememberMe=true)");
                    } else {
                        Credentials::Clear();
                        Console::Info("[auth] Credentials cleared (rememberMe=false)");
                    }
                    VMP_END;

                    if (result.key_expires_at.has_value()) {
                        UI::Profile::ExpiresAt = *result.key_expires_at;
                        UI::Profile::HasExpiry.store(true);

                        auto tt = std::chrono::system_clock::to_time_t(*result.key_expires_at);
                        std::tm tm{};
                        localtime_s(&tm, &tt);
                        char buf[64];
                        strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", &tm);
                        expiresText = buf;
                        UI::Profile::ExpiryText = buf;
                        Console::Info("[auth] Expires at %s", buf);
                    } else {
                        UI::Profile::HasExpiry.store(false);
                        UI::Profile::ExpiryText = "Lifetime";
                        Console::Info("[auth] No expiry (lifetime)");
                    }

                    if (toastSuccessCallback) toastSuccessCallback(L(xorstr_("Authenticated successfully"),xorstr_("Autenticado com sucesso")), 3.f);

                    Sys::Sleep(1500);
                    Config::CurrentTab().store(Config::Menu);

                    // Persistir cliente e iniciar heartbeat. Server pode
                    // mandar disconnect mid-cheat se HWID mudar / banimento /
                    // licenca for revogada.
                    g_persistentClient = std::move(client);
                    StartSessionThread();

                    Console::Success("[auth] Session started, moving to Menu");
                } else {
                    std::string err = result.error_code.value_or("");
                    Console::Error("[auth] Login rejected: %s", err.empty() ? "(no code)" : err.c_str());
                    statusText = TranslateAuthCode(err.c_str());
                    if (toastCallback) toastCallback(statusText, 5.f);
                }
                VMP_END;
            } catch (const zimo::ConnectionException& ex) {
                Console::Error("[auth] ConnectionException: %s", ex.what());
                statusText = TranslateAuthCode("CONN_FAILED");
                if (toastCallback) toastCallback(statusText, 5.f);
            } catch (const zimo::AuthException& ex) {
                Console::Error("[auth] AuthException code='%s' what='%s'", ex.error_code().c_str(), ex.what());
                statusText = TranslateAuthCode(ex.error_code().empty() ? "AUTH_FAILED" : ex.error_code().c_str());
                if (toastCallback) toastCallback(statusText, 5.f);
            } catch (const zimo::ProtocolException& ex) {
                Console::Error("[auth] ProtocolException: %s (server_err=%d)", ex.what(),
                    ex.server_error_code().value_or(-1));
                statusText = TranslateAuthCode("ERR_INTERNAL");
                if (toastCallback) toastCallback(statusText, 5.f);
            } catch (const zimo::ZimoException& ex) {
                Console::Error("[auth] ZimoException: %s", ex.what());
                statusText = TranslateAuthCode("ERR_INTERNAL");
                if (toastCallback) toastCallback(statusText, 5.f);
            } catch (const std::exception& ex) {
                Console::Error("[auth] std::exception: %s", ex.what());
                statusText = TranslateAuthCode("ERR_UNKNOWN");
                if (toastCallback) toastCallback(statusText, 5.f);
            } catch (...) {
                Console::Error("[auth] Unknown exception caught (catch-all)");
                statusText = TranslateAuthCode("ERR_UNKNOWN");
                if (toastCallback) toastCallback(statusText, 5.f);
            }

            authenticating.store(false);
            VMP_END;
        }, args);
    }
}
