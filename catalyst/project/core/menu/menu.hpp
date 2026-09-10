#pragma once

class menu
{
public:
	// Deve ser chamado uma única vez após o contexto ImGui ser criado.
	// Aplica o ImGuiStyle global (fora do loop de render).
	void on_init( );
	void draw( );
	[[nodiscard]] bool is_open( ) const noexcept { return this->m_open; }

private:
	void draw_aimbot( );
	void draw_triggerbot( );
	void draw_rcs( );
	void draw_esp( );
	void draw_misc( );
	void draw_settings( );

	bool m_open{};
};
