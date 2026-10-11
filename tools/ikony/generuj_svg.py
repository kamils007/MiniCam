# Generator ikon MiniCAM: rysuje SVG 32x32 (czerwona geometria, złote narzędzia, szare bryły, niebieskie strzałki).
# Użycie: python3 tools/ikony/generuj_svg.py resources/ikony, potem node tools/ikony/renderuj_png.js resources/ikony
import sys,os
out=sys.argv[1]
R='#C62828';RD='#7F1414'      # geometria
G='#F4B400';GD='#8A6300'      # złoto
B='#1E6FD9';BD='#0D3F85'      # strzałki
S1,S2,S3,SD='#E6E6E6','#BDBDBD','#8E8E8E','#4A4A4A'  # bryła
OK='#2E9E3E'
def arrow(d,c=B,cd=BD,w=2.6):
    return f'<path d="{d}" fill="none" stroke="{cd}" stroke-width="{w+1.4}" stroke-linecap="round" stroke-linejoin="round"/><path d="{d}" fill="none" stroke="{c}" stroke-width="{w}" stroke-linecap="round" stroke-linejoin="round"/>'
def geo(d,w=2.6,dash=None,c=R):
    da=f' stroke-dasharray="{dash}"' if dash else ''
    return f'<path d="{d}" fill="none" stroke="{c}" stroke-width="{w}" stroke-linecap="round" stroke-linejoin="round"{da}/>'
def head(x,y,ang,c=B,cd=BD,s=1):
    return f'<g transform="translate({x} {y}) rotate({ang}) scale({s})"><path d="M0 0 L-7 -5 L-7 5 Z" fill="{c}" stroke="{cd}" stroke-width="1.2" stroke-linejoin="round"/></g>'
def cube(x,y,s=1):
    return (f'<g transform="translate({x} {y}) scale({s})" stroke="{SD}" stroke-width="1" stroke-linejoin="round">'
            f'<path d="M0 4 L8 0 L16 4 L8 8 Z" fill="{S1}"/><path d="M0 4 L8 8 L8 17 L0 13 Z" fill="{S2}"/><path d="M16 4 L8 8 L8 17 L16 13 Z" fill="{S3}"/></g>')
def page(x,y,w=13,h=17,fill='#FFFFFF'):
    f=4
    return f'<path d="M{x} {y} H{x+w-f} L{x+w} {y+f} V{y+h} H{x} Z" fill="{fill}" stroke="{SD}" stroke-width="1.2" stroke-linejoin="round"/><path d="M{x+w-f} {y} V{y+f} H{x+w}" fill="#DDD" stroke="{SD}" stroke-width="1" stroke-linejoin="round"/>'
I={}
I['otworz-model']=(f'<path d="M2 8 H11 L13 10 H26 V27 H2 Z" fill="{G}" stroke="{GD}" stroke-width="1.2" stroke-linejoin="round"/>'
  + cube(14,3,1.0) +
  f'<path d="M2 27 L6 14 H30 L26 27 Z" fill="#FFD54F" stroke="{GD}" stroke-width="1.2" stroke-linejoin="round"/>')
I['dopasuj-do-okna']=(cube(8,7,1.0)+''.join(geo(d,2.4,c=B) for d in
  ['M3 9 V3 H9','M23 3 H29 V9','M29 23 V29 H23','M9 29 H3 V23']))
I['okno-warstwy']=(f'<g stroke-width="1.2" stroke-linejoin="round">'
  f'<path d="M16 18 L29 23 L16 28 L3 23 Z" fill="#7E57C2" stroke="#3E2A6B"/>'
  f'<path d="M16 11 L29 16 L16 21 L3 16 Z" fill="{OK}" stroke="#145A20"/>'
  f'<path d="M16 4 L29 9 L16 14 L3 9 Z" fill="#42A5F5" stroke="{BD}"/></g>')
I['cofnij']=arrow('M25 25 C27 15 20 9 9 11')+head(7,11.3,180)
I['ponow']=arrow('M7 25 C5 15 12 9 23 11')+head(25,11.3,0)
I['usun']=(
  f'<path d="M9 9 L23 23 M23 9 L9 23" stroke="{RD}" stroke-width="6" stroke-linecap="round"/><path d="M9 9 L23 23 M23 9 L9 23" stroke="#E53935" stroke-width="3.6" stroke-linecap="round"/>')
I['kopiuj']=page(4,3)+page(14,11)+geo('M17 20 H24 M17 24 H24',1.6)
I['wytnij']=(f'<g stroke="{SD}" stroke-width="2.4" stroke-linecap="round"><path d="M10 3 L21 21"/><path d="M22 3 L11 21"/></g>'
  f'<circle cx="9" cy="25" r="4.2" fill="#FFF" stroke="{R}" stroke-width="2.6"/><circle cx="23" cy="25" r="4.2" fill="#FFF" stroke="{R}" stroke-width="2.6"/>'
  f'<circle cx="16" cy="12" r="1.3" fill="{SD}"/>')
I['wklej']=(f'<rect x="4" y="5" width="18" height="24" rx="2" fill="#B07A3B" stroke="#5C3A12" stroke-width="1.2"/>'
  f'<rect x="9" y="3" width="8" height="5" rx="1.2" fill="{S2}" stroke="{SD}" stroke-width="1.1"/>'
  + page(14,12,14,17) + geo('M17 21 H25 M17 25 H25',1.6))
I['przesun']=(geo('M3 15 H13 V27 H3 Z',1.6,'2 2.2',c=S3)+geo('M18 4 H28 V16 H18 Z',2.6)
  +arrow('M10 20 L19 11',w=2.2)+head(21,9,-45,s=0.85))
I['obroc']=(geo('M5 27 H17 V22',1.6,'2 2.2',c=S3)+geo('M5 27 L13 15 L17 18',2.6)
  +f'<circle cx="5" cy="27" r="2.2" fill="{G}" stroke="{GD}" stroke-width="1"/>'
  +arrow('M22 26 C28 18 24 9 15 6',w=2.2)+head(13,5.5,195,s=0.85))
I['lustro']=(f'<path d="M16 2 V30" stroke="{B}" stroke-width="1.6" stroke-dasharray="3 2.2"/>'
  +geo('M13 7 L3 25 H13 Z',2.4)+geo('M19 7 L29 25 H19 Z',1.6,'2 2.2',c=S3))
I['skaluj']=(geo('M3 19 H13 V29 H3 Z',2.2)+geo('M3 7 H25 V29',2.6)+geo('M3 7 V19',2.6,'2 2.2',c=S3)
  +arrow('M10 22 L20 12',w=2.2)+head(22,10,-45,s=0.85))
I['przerwij']=(geo('M3 24 L13 14',2.6)+geo('M19 18 L29 8',2.6)
  +f'<path d="M12 9 L16 15 L14 16 L20 23" fill="none" stroke="{G}" stroke-width="3.6" stroke-linejoin="round" stroke-linecap="round"/><path d="M12 9 L16 15 L14 16 L20 23" fill="none" stroke="{GD}" stroke-width="1" stroke-linejoin="round"/>')
I['przytnij']=(geo('M4 22 H28',2.6)+geo('M10 28 L16 22',2.6)+geo('M16 22 L24 4',1.6,'2 2.2',c=S3)
  +f'<path d="M18 8 L28 14 M28 8 L18 14" stroke="{RD}" stroke-width="3.6" stroke-linecap="round"/><path d="M18 8 L28 14 M28 8 L18 14" stroke="#E53935" stroke-width="2" stroke-linecap="round"/>')
I['polacz']=(geo('M3 26 L14 14',2.6)+geo('M14 14 L29 10',2.6)
  +f'<circle cx="14" cy="14" r="4" fill="{OK}" stroke="#145A20" stroke-width="1.2"/><path d="M12 14 L13.6 15.8 L16.4 12" fill="none" stroke="#FFF" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>')
I['rozpoznaj-cechy']=(f'<g stroke="{SD}" stroke-width="1" stroke-linejoin="round">'
  f'<path d="M2 12 L14 6 L28 12 L16 18 Z" fill="{S1}"/><path d="M2 12 L16 18 L16 26 L2 20 Z" fill="{S2}"/><path d="M28 12 L16 18 L16 26 L28 20 Z" fill="{S3}"/></g>'
  f'<path d="M9 12 L14 9.5 L20 12 L15 14.5 Z" fill="{S3}" stroke="{R}" stroke-width="2" stroke-linejoin="round"/>'
  f'<circle cx="23" cy="22" r="5" fill="#E3F2FD" fill-opacity=".85" stroke="{GD}" stroke-width="2.4"/><path d="M26.5 25.5 L30 29" stroke="{GD}" stroke-width="3.2" stroke-linecap="round"/>')
I['auto-wyrownanie']=(f'<path d="M1 24 L13 18 L31 24 L19 30 Z" fill="#BBDEFB" stroke="{BD}" stroke-width="1" stroke-linejoin="round"/>'
  + cube(8,8,1.0) + arrow('M27 17 C30 10 26 4 19 4',w=2)+head(17,4,180,s=0.75))
I['ustawienia-rozpoznawania']=(
  '<g transform="translate(13 13)">' + ''.join(f'<rect x="-2.6" y="-12" width="5.2" height="6" rx="1" fill="{G}" stroke="{GD}" stroke-width="1" transform="rotate({a})"/>' for a in range(0,360,45))
  + f'<circle r="8" fill="{G}" stroke="{GD}" stroke-width="1.2"/><circle r="3.2" fill="#FFF" stroke="{GD}" stroke-width="1.2"/></g>'
  + f'<rect x="17" y="17" width="13" height="13" rx="1.5" fill="#FFF" stroke="{SD}" stroke-width="1"/>' + geo('M20 27 V20 H27 V27 Z',2.2))
# Rysowanie 2D (zakładka Geometria): czerwona geometria, złote kwadraciki = punkty, które się wskazuje.
K='#212121'  # czarne linie pomocnicze (promień, średnica)
def pt(x,y):
    return f'<rect x="{x-2.6}" y="{y-2.6}" width="5.2" height="5.2" fill="{G}" stroke="{GD}" stroke-width="1"/>'
def aux(d,dash=None):
    da=f' stroke-dasharray="{dash}"' if dash else ''
    return f'<path d="{d}" fill="none" stroke="{K}" stroke-width="1.6" stroke-linecap="round"{da}/>'
def tip(x,y,ang):
    return f'<g transform="translate({x} {y}) rotate({ang})"><path d="M0 0 L-5 -3 L-5 3 Z" fill="{K}"/></g>'
ARC='M5 8 A20 20 0 0 1 25 27'
I['linia']=geo('M4 22 L11 6 L19 26 L28 9')+pt(4,22)+pt(11,6)+pt(19,26)+pt(28,9)
I['prostokat']=geo('M5 9 H27 V24 H5 Z')+pt(5,9)+pt(27,9)+pt(27,24)+pt(5,24)
I['luk-3-punkty']=geo(ARC)+pt(5,8)+pt(18.8,13.5)+pt(25,27)
I['luk-2-punkty-promien']=geo(ARC)+aux('M8 25 L17.5 15.5')+tip(19,14,-45)+tip(6.5,26.5,135)+pt(5,8)+pt(25,27)
I['luk-2-punkty-srodek']=geo(ARC)+aux('M5 28 L25 27')+pt(5,8)+pt(5,28)+pt(25,27)
I['luk-srodek-kat']=geo(ARC)+aux('M5 28 L5 8','1.6 2.4')+aux('M5 28 L18.8 13.5')+pt(5,8)+pt(5,28)
CIRC='<circle cx="16" cy="16" r="11" fill="none" stroke="'+R+'" stroke-width="2.6"/>'
I['kolo-srodek-srednica']=CIRC+aux('M10.5 21.5 L21.5 10.5')+tip(23,9,-45)+tip(9,23,135)+pt(16,16)
I['kolo-srodek-promien']=CIRC+aux('M16 16 L22.5 9.5')+tip(24,8,-45)+pt(16,16)
I['kolo-srodek-punkt']=CIRC+pt(16,16)+pt(23.8,8.2)
I['kolo-2-punkty']=CIRC+pt(8.2,23.8)+pt(23.8,8.2)
I['kolo-3-punkty']=CIRC+pt(5,16)+pt(23.8,8.2)+pt(19.4,26.5)
os.makedirs(out,exist_ok=True)
for k,v in I.items():
    open(f'{out}/{k}.svg','w').write(f'<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 32 32">{v}</svg>\n')
print(len(I))
