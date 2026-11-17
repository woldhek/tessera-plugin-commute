# Commute

> Needs the Home Assistant integration [Commute](https://github.com/woldhek/ha-commute) (HACS): it follows each
> person, and this plugin draws what it knows.

How far someone is on the way between work and home, how they travel right now (walking, bike, train, metro, tram or
bus, car), when they will probably arrive, and **what that guess is based on**: the speed between the last two places,
the phone's activity sensor, or the NS train they are on.

- **Commute**, one person. A single cell is a ring in the colours of the legs, with the way of travelling in the
  middle. A wide tile is a ribbon from where they left to where they are going: each leg in its own colour, a disc
  where they are now, the arrival on the right and a line of what the guess is based on under it. A large tile adds
  the legs with their times. The tile is quiet ("No commute") while nobody is on the way.
- **Commutes**, up to four people, a lane each, those on the way first.
- **Trip**, the card a tap opens: a map of the trip so far, drawn by the screen from the places the integration keeps
  (no map from the internet), with the legs, their times, and what the guess is based on.
- **Commute in the top bar**: an item that is only there while a commute is under way ("Anton 55%", with the icon of
  how they travel).

The colours: walking green, bike amber, train blue, metro, tram or bus purple, car slate. Tessera's icons have no
walking figure, metro or office, so a runner, a bus and a briefcase stand in for walking, public transport and work.

Nothing leaves your home: the places stay in Home Assistant, and the screen gets them from there.

## Set up

1. Install the Home Assistant integration **Commute** through HACS and add a person with their home and work zone
   (its README says how). Check that `sensor.<name>_commute` exists.
2. Add this plugin to a screen in Tessera (**Plugins**, then the repository's link). When it asks for **Commute in the
   top bar**, choose the commute sensor of the person the top bar should show, or leave it. The screen builds once.
3. In **Layout**, place the **Commute** tile from the library's Plugins group, and choose the person's commute sensor
   in the inspector. For several people, place **Commutes** and choose up to four.
4. To see it in the top bar, add **Commute** under "From plugins" to a page's top bar.
5. Tap the tile to see the map of the trip.

## Good to know

- An iPhone sends its place every few hundred metres or minutes, so the percentage moves in steps, most of all on a
  train. The phone's activity sensor makes the guesses much surer.
- The percentage is how much closer to the end they are than at the start, in a straight line: a walk out of the
  station barely moves it.
- The arrival comes from how long the commute usually takes and how far along it is now.

---

## Nederlands

> Werkt met de Home Assistant-integratie [Commute](https://github.com/woldhek/ha-commute) (HACS): die volgt ieder
> persoon, deze plugin tekent wat die weet.

Hoe ver iemand is tussen werk en thuis, hoe die nu reist (lopen, fiets, trein, metro, tram of bus, auto), wanneer
die waarschijnlijk aankomt, en **waarop die schatting rust**: de snelheid tussen de laatste twee plekken, de
activiteitssensor van de telefoon, of de NS-trein waar die in zit.

- **Commute**, één persoon. Eén cel is een ring in de kleuren van de delen van de reis, met het vervoersmiddel in het
  midden. Een brede tegel is een lint van vertrek naar bestemming: ieder deel in zijn kleur, een stip waar iemand nu
  is, rechts de aankomst en eronder waarop de schatting rust. Een grote tegel toont ook de delen met hun tijden. Is
  niemand onderweg, dan zegt de tegel rustig "Niet onderweg".
- **Commutes**, tot vier personen, ieder een eigen baan, wie onderweg is bovenaan.
- **Reis**, de kaart na een tik: een kaart van de reis tot nu toe, door het scherm getekend uit de plekken die de
  integratie bewaart (geen landkaart van internet), met de delen, hun tijden en waarop de schatting rust.
- **Woon-werkreis in de bovenbalk**: een item dat er alleen is tijdens een reis ("Anton 55%", met het icoon van het
  vervoersmiddel).

De kleuren: lopen groen, fiets amber, trein blauw, metro, tram of bus paars, auto leisteen. Tessera heeft geen icoon
voor lopen, metro of kantoor; een rennend poppetje, een bus en een aktetas staan ervoor in de plaats.

Niets gaat de deur uit: de plekken blijven in Home Assistant, en het scherm krijgt ze daarvandaan.

### Instellen

1. Installeer de Home Assistant-integratie **Commute** via HACS en voeg een persoon toe met de thuis- en werkzone
   (de README daar legt het uit). Kijk of `sensor.<naam>_commute` bestaat.
2. Voeg deze plugin in Tessera aan een scherm toe (**Plugins**, dan de link van de repository). Vraagt hij om
   **Woon-werkreis in de bovenbalk**, kies dan de commute-sensor van wie de bovenbalk moet tonen, of laat het staan.
   Het scherm bouwt één keer.
3. Zet in **Layout** de tegel **Commute** uit de groep Plugins neer, en kies in de inspector de commute-sensor van de
   persoon. Voor meer personen: zet **Commutes** neer en kies er tot vier.
4. Voor de bovenbalk: voeg **Woon-werkreis** onder "From plugins" toe aan de bovenbalk van een pagina.
5. Tik op de tegel voor de kaart van de reis.

### Goed om te weten

- Een iPhone stuurt zijn plek om de paar honderd meter of minuten; het percentage loopt dus in stappen, vooral in de
  trein. Met de activiteitssensor wordt de gok veel zekerder.
- Het percentage is hoeveel dichter bij de bestemming iemand is dan bij vertrek, in rechte lijn: een stukje lopen
  vanaf het station verandert het nauwelijks.
- De aankomst komt uit hoe lang de reis meestal duurt en hoe ver die nu is.
