// Isolated Studio 13 symbol family. Never changes the shared 57-name Icons.js.
// npm install --prefix .cache/glyphgen --no-audit --no-fund @resvg/resvg-js@2.6.2
const fs=require('fs'),path=require('path');
const {Resvg}=require('../../.cache/glyphgen/node_modules/@resvg/resvg-js');
const out=path.resolve(__dirname,'../../assets/full-emulator-ui/StudioIcons');fs.mkdirSync(out,{recursive:true});
const shapes={
 gauge:'<path d="M3 18a10 10 0 1 1 18 0"/><path d="M12 3v2M5 7l1.5 1.5M3 13h2m14 0h2M18 7l-5 6"/><circle cx="12" cy="14" r="2"/>',
 performance:'<path fill="white" stroke="none" d="m13 2-9 12h6l-1 8 11-14h-7l2-6Z"/>',
 display:'<rect x="3" y="4" width="18" height="13" rx="2"/><path d="M8 21h8m-4-4v4"/>',
 graphics:'<path d="M12 3a9 9 0 1 0 0 18h1.5a2 2 0 0 0 1.5-3.3 1.5 1.5 0 0 1 1.1-2.5H18a3 3 0 0 0 3-3c0-5.2-4-9.2-9-9.2Z"/><path d="M7 11h.01M9 7h.01m5 0h.01m3 4h.01" stroke-width="3"/>',
 palette:'<path d="m8 3-5 3-2 5 4 2 1-3v11h12V10l1 3 4-2-2-5-5-3c-1 4-7 4-8 0Z"/>',
 gamesettings:'<path d="M7 7h10a4 4 0 0 1 4 3l1 7a2.6 2.6 0 0 1-4.2 2.3L15 17H9l-2.8 2.3A2.6 2.6 0 0 1 2 17l1-7a4 4 0 0 1 4-3Z"/><path d="M7 10v5m-2.5-2.5h5m6-1h.01m3 3h.01"/>',
 devices:'<path d="M4 15v-4a8 8 0 0 1 16 0v4"/><rect x="3" y="12" width="4" height="8" rx="2"/><rect x="17" y="12" width="4" height="8" rx="2"/>',
 gamepad:'<path d="M7 7h10a4 4 0 0 1 4 3l1 7a2.6 2.6 0 0 1-4.2 2.3L15 17H9l-2.8 2.3A2.6 2.6 0 0 1 2 17l1-7a4 4 0 0 1 4-3Z"/><path d="M7 10v5m-2.5-2.5h5m6-1h.01m3 3h.01"/>',
 preferences:'<path d="M3 6h4m4 0h10M3 12h10m4 0h4M3 18h4m4 0h10"/><circle cx="9" cy="6" r="2"/><circle cx="15" cy="12" r="2"/><circle cx="9" cy="18" r="2"/>',
 phone:'<rect x="6" y="2" width="12" height="20" rx="3"/><path d="M10 5h4m-3 14h2"/>',
 shortcuts:'<rect x="2" y="5" width="20" height="14" rx="2"/><path d="M6 9h.01M10 9h.01M14 9h.01M18 9h.01M6 12h.01M10 12h.01M14 12h.01M18 12h.01M7 16h10"/>',
 userdata:'<ellipse cx="12" cy="5" rx="8" ry="3"/><path d="M4 5v7c0 4 16 4 16 0V5M4 12v7c0 4 16 4 16 0v-7"/>',
 advanced:'<path d="m10 2-1 3-3 1-3 5 2 2v3l3 2 1 3h6l1-3 3-2v-3l2-2-3-5-3-1-1-3Z"/><circle cx="12" cy="12" r="3"/>',
 about:'<circle cx="12" cy="12" r="9"/><path d="M12 11v6m0-10h.01"/>',
 close:'<path d="m6 6 12 12M6 18 18 6"/>',
 headphones:'<path d="M4 15v-4a8 8 0 0 1 16 0v4"/><rect x="3" y="12" width="4" height="8" rx="2"/><rect x="17" y="12" width="4" height="8" rx="2"/>',
 cpu:'<rect x="5" y="5" width="14" height="14" rx="2"/><rect x="8" y="8" width="8" height="8" rx="1"/><path d="M8 2v3m4-3v3m4-3v3M8 19v3m4-3v3m4-3v3M2 8h3m-3 4h3m-3 4h3m14-8h3m-3 4h3m-3 4h3"/>',
 memory:'<path d="M2 6h20v12H2Zm3 12v3m4-3v3m6-3v3m4-3v3"/><path d="M6 9v5m4-5v5m4-5v5m4-5v5"/>',
 rocket:'<path d="M9 15c-3-5 2-12 12-12 0 10-7 15-12 12Z"/><path d="m9 8-5 1-2 6 6-1m8 1-1 5-6 2 1-6M6 18l-3 3"/><circle cx="16" cy="8" r="2"/>',
 framerate:'<rect x="3" y="3" width="18" height="14" rx="2"/><path d="M8 21h8m-4-4v4M7 13l3-4 3 2 4-5"/>',
 brand:'<path fill="white" stroke="none" d="M1 4h6l6 9-3 5Zm8 0h14L12 21l-3-5 5-7h-2Z"/>',
 chevrondown:'<path d="m5 9 7 7 7-7"/>',
 minimize:'<path d="M5 12h14"/>',
 sync:'<path d="M20 8A8 8 0 0 0 6 5L3 8m0-5v5h5M4 16a8 8 0 0 0 14 3l3-3m0 5v-5h-5"/>',
 check:'<path d="m4 12 5 5L20 6"/>',
 layers:'<rect x="3" y="3" width="13" height="11" rx="2"/><path d="M8 14v5a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V10a2 2 0 0 0-2-2h-4"/>'
};
for(const [name,shape] of Object.entries(shapes)){
 const svg=`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">${shape}</svg>`;
 fs.writeFileSync(path.join(out,name+'.png'),new Resvg(svg,{fitTo:{mode:'width',value:64}}).render().asPng());
}
console.log('Generated',Object.keys(shapes).length,'isolated Studio symbols.');
