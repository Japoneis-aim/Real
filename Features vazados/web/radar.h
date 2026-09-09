#pragma once

inline const char* RADAR_HTML = R"html(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>CS2 League - Radar</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#0a0a0f;color:#e0e0e0;font-family:'Segoe UI',sans-serif;display:flex;height:100vh;overflow:hidden}
#sidebar{width:260px;background:#12121a;padding:16px;border-right:1px solid #1e1e2e;display:flex;flex-direction:column;gap:10px}
#sidebar h1{font-size:14px;color:#7c7cff;letter-spacing:3px;text-transform:uppercase}
.info{font-size:11px;color:#555}.info span{color:#aaa}
.player-list{flex:1;overflow-y:auto;display:flex;flex-direction:column;gap:3px}
.player{display:flex;align-items:center;gap:8px;padding:5px 8px;border-radius:4px;font-size:11px;background:#1a1a24}
.player.team{border-left:3px solid #4af}.player.enemy{border-left:3px solid #f44}.player.local{border-left:3px solid #5f5;background:#1a2a1a}
.player .name{flex:1;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.player .hp{width:32px;text-align:right;font-weight:bold;font-size:10px}
.player .hp.high{color:#5f5}.player .hp.mid{color:#fa0}.player .hp.low{color:#f55}
.player .dist{width:40px;text-align:right;color:#555;font-size:10px}
.player .money-tag{color:#fc0;font-size:10px;width:44px;text-align:right}
#radar-wrap{flex:1;display:flex;align-items:center;justify-content:center;background:#08080e}
canvas{border:1px solid #1a1a2a;border-radius:4px}
.status{padding:6px 10px;background:#1a1a24;border-radius:4px;font-size:10px}
.status.on{border-left:3px solid #5f5}.status.off{border-left:3px solid #f55}
</style>
</head>
<body>
<div id="sidebar">
  <h1>CS2 League</h1>
  <div id="status" class="status off">Conectando...</div>
  <div class="info">Mapa: <span id="map">-</span></div>
  <div class="info">Jogadores: <span id="count">0</span></div>
  <hr style="border-color:#1e1e2e">
  <div class="player-list" id="players"></div>
</div>
<div id="radar-wrap">
  <canvas id="radar" width="720" height="720"></canvas>
</div>
<script>
const C=document.getElementById('radar'),X=C.getContext('2d'),W=720;
let data=null,mapImg=null,mapData=null,curMap='',mapUnsupported=false;

async function fetchData(){
  try{
    const r=await fetch('/api/data');
    data=await r.json();
    document.getElementById('status').className='status on';
    document.getElementById('status').textContent='Conectado';
    document.getElementById('map').textContent=(data.map||'-').replace(/^maps\//,'').replace(/\.vpk$/,'');
    document.getElementById('count').textContent=data.players.length;
    updateList();
    if(data.map&&data.map!==curMap){curMap=data.map;let m=data.map.replace(/^maps\//,'').replace(/\.vpk$/,'');loadMap(m)}
  }catch(e){
    document.getElementById('status').className='status off';
    document.getElementById('status').textContent='Desconectado';
    data=null;
  }
}

function loadMap(name){
  mapImg=new Image();
  mapImg.src='/maps/'+name+'/radar.png';
  mapImg.onerror=()=>{mapImg=null};
  fetch('/maps/'+name+'/data.json').then(r=>{
    if(!r.ok)throw 0;
    return r.json();
  }).then(d=>{mapData=d;mapUnsupported=false}).catch(()=>{mapData=null;mapUnsupported=true});
}

function updateList(){
  if(!data)return;
  const el=document.getElementById('players');
  el.innerHTML='';
  const s=[...data.players].sort((a,b)=>a.distance-b.distance);
  for(const p of s){
    const c=p.isLocal?'local':p.isTeam?'team':'enemy';
    const h=p.health>60?'high':p.health>25?'mid':'low';
    el.innerHTML+=`<div class="player ${c}"><span class="name">${p.name||'?'}</span><span class="money-tag">$${p.money}</span><span class="dist">${p.distance}m</span><span class="hp ${h}">${p.health}</span></div>`;
  }
}

function worldToRadar(wx,wy){
  if(mapData){
    const px=(wx-mapData.x)/(mapData.scale);
    const py=(mapData.y-wy)/(mapData.scale);
    return{x:px*(W/1024),y:py*(W/1024)};
  }
  // Fallback player-relative: local no centro, +X mundial = direita, +Y = cima.
  // Range 50m * 39.37 units/m, rotacionado pelo yaw do player pra "frente" = topo.
  if(!data||!data.localPosition)return null;
  const lp=data.localPosition,R=W*0.45,RANGE=50*39.37;
  const wxr=wx-lp.x,wyr=wy-lp.y;
  const yaw=(data.localYaw||0)*Math.PI/180;
  const a=Math.PI/2-yaw,ca=Math.cos(a),sa=Math.sin(a);
  const dx=wxr*ca-wyr*sa,dy=wxr*sa+wyr*ca;
  const sx=dx*R/RANGE,sy=-dy*R/RANGE;
  return{x:W/2+sx,y:W/2+sy,fallback:true};
}

function draw(){
  X.clearRect(0,0,W,W);
  X.fillStyle='#0d0d15';
  X.fillRect(0,0,W,W);

  if(mapImg&&mapImg.complete&&mapImg.naturalWidth>0&&mapData){
    X.globalAlpha=0.7;
    X.drawImage(mapImg,0,0,W,W);
    X.globalAlpha=1;
  }else{
    // Sem mapa: estilo radar circular igual o preview do imgui (concentricos
    // + cruz). Topo do circulo = direcao que o player olha.
    const cx=W/2,cy=W/2,R=W*0.45;
    X.fillStyle='rgba(20,20,28,0.85)';
    X.beginPath();X.arc(cx,cy,R,0,Math.PI*2);X.fill();
    X.strokeStyle='rgba(60,60,70,0.5)';X.lineWidth=1;
    X.beginPath();X.arc(cx,cy,R,0,Math.PI*2);X.stroke();
    X.beginPath();X.arc(cx,cy,R*0.66,0,Math.PI*2);X.stroke();
    X.beginPath();X.arc(cx,cy,R*0.33,0,Math.PI*2);X.stroke();
    X.beginPath();X.moveTo(cx-R,cy);X.lineTo(cx+R,cy);X.stroke();
    X.beginPath();X.moveTo(cx,cy-R);X.lineTo(cx,cy+R);X.stroke();
    // "N" do norte mundial que orbita conforme o yaw — sinaliza rotacao.
    if(data){
      const yaw=(data.localYaw||0)*Math.PI/180,a=Math.PI/2-yaw;
      const nx=0*Math.cos(a)-1*Math.sin(a),ny=0*Math.sin(a)+1*Math.cos(a);
      const nr=R-18;
      X.fillStyle='rgba(180,180,200,0.85)';X.font='bold 14px Segoe UI';X.textAlign='center';
      X.fillText('N',cx+nx*nr,cy-ny*nr+5);
    }
  }

  if(!data||!data.players.length){
    X.fillStyle='#333';X.font='13px Segoe UI';X.textAlign='center';
    X.fillText('Aguardando jogadores...',W/2,W/2);
    requestAnimationFrame(draw);return;
  }

  if(!mapData&&mapUnsupported){
    X.fillStyle='#555';X.font='11px Segoe UI';X.textAlign='left';
    X.fillText('Mapa "'+curMap.replace(/^maps\//,'').replace(/\.vpk$/,'')+'" não suportado',10,W-10);
  }else if(!mapData){
    X.fillStyle='#444';X.font='11px Segoe UI';X.textAlign='left';
    X.fillText('Carregando mapa...',10,W-10);
  }

  for(const p of data.players){
    let pt=worldToRadar(p.position.x,p.position.y);
    if(!pt)continue;
    // Fallback: clampa pra borda do circulo de raio R em vez de cortar.
    if(pt.fallback){
      const cx=W/2,cy=W/2,R=W*0.45-4;
      const dx=pt.x-cx,dy=pt.y-cy,d=Math.sqrt(dx*dx+dy*dy);
      if(d>R){pt.x=cx+dx*R/d;pt.y=cy+dy*R/d}
    } else if(pt.x<0||pt.x>W||pt.y<0||pt.y>W){continue}

    const color=p.isLocal?'#5fff5f':p.isTeam?'#44aaff':'#ff4444';
    const sz=p.isLocal?6:5;

    X.fillStyle=color;
    X.shadowColor=color;
    X.shadowBlur=12;
    X.beginPath();
    X.arc(pt.x,pt.y,sz,0,Math.PI*2);
    X.fill();
    X.shadowBlur=0;

    if(p.isLocal){
      // No fallback player-relative, "cima" ja eh a frente do player,
      // entao o arrow aponta sempre pra cima. Caso contrario usa yaw.
      let ax,ay;
      if(pt.fallback){ax=0;ay=-18}
      else{
        const yaw=(data.localYaw||0)*Math.PI/180;
        ax=Math.cos(yaw)*18;ay=-Math.sin(yaw)*18;
      }
      X.strokeStyle='rgba(95,255,95,0.7)';
      X.lineWidth=2;
      X.beginPath();
      X.moveTo(pt.x,pt.y);
      X.lineTo(pt.x+ax,pt.y+ay);
      X.stroke();
    }

    X.fillStyle='#ddd';
    X.font='9px Segoe UI';
    X.textAlign='center';
    X.shadowColor='#000';X.shadowBlur=3;
    X.fillText(p.name||'',pt.x,pt.y-10);
    X.shadowBlur=0;

    const bw=20,bh=2.5,bx=pt.x-bw/2,by=pt.y+9;
    X.fillStyle='rgba(0,0,0,0.5)';
    X.fillRect(bx,by,bw,bh);
    X.fillStyle=p.health>60?'#5f5':p.health>25?'#fa0':'#f55';
    X.fillRect(bx,by,bw*(p.health/100),bh);
  }

  requestAnimationFrame(draw);
}

setInterval(fetchData,50);
requestAnimationFrame(draw);
</script>
</body>
</html>
)html";
