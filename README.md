# Engine Simulator
![Engine Simulator screenshot](docs/public/screenshots/screenshot_v01.png?raw=true)

A real-time internal combustion engine simulator **designed to produce engine audio and simulate engine response characteristics.** It is not a scientific tool and its figures shouldn't be used for engineering or tuning decisions.

Engine Simulator was created by [AngeTheGreat (Ange Yaghi)](https://github.com/ange-yaghi/engine-sim) for his YouTube videos. The original repository is no longer updated; this fork keeps the open source version alive. It runs natively on **Windows, Linux and macOS**, adds rotary engines and drivetrain features from the later releases, incorporates the script library improvements of the [Community Edition](https://github.com/Engine-Simulator/engine-sim-community-edition), and fixes a long list of bugs. See the [changelog](CHANGELOG.md) for details.

## Download

Prebuilt packages are attached to each [release](https://github.com/kyan0s-git/engine-sim/releases):

| Platform | Package | How to run |
| :-- | :-- | :-- |
| Windows (x64) | `engine-sim-<version>-windows-x64.zip` | Extract and run `bin/engine-sim-app.exe` |
| Linux (x64) | `engine-sim-<version>-linux-x64.tar.gz` | Extract and run `bin/engine-sim-app` (or `engine-sim.desktop`) |
| macOS (Apple Silicon and Intel) | `engine-sim-<version>-macos-universal.zip` | Extract and open `Engine Simulator.app` |

The packages are self-contained. A GPU driver supporting OpenGL 4.1 or newer is required. On macOS the app is not notarized, so the first time you open it right-click it and choose *Open*. The Linux package is built on Ubuntu 22.04 and works on distributions with glibc 2.35 or newer; it supports both X11 and Wayland.

## Controls

| Key/Input | Action |
| :---: | :--- |
| A | Toggle ignition |
| S | Hold for starter |
| Q, W, E, R | Throttle presets |
| Space + Scroll | Fine throttle adjustment |
| Up / Down Arrow | Shift up / down |
| Shift | Hold to depress the clutch (hold Space as well to engage/disengage slowly) |
| T / U | Gradually release / engage the clutch (Y holds the current position) |
| . | Hold to apply the brakes |
| D | Toggle the dyno |
| H | Toggle RPM hold (see below) |
| G + Scroll | Change the hold speed |
| Z + Scroll | Volume |
| X + Scroll | Convolution level |
| C + Scroll | High frequency gain |
| V + Scroll | Low frequency noise |
| B + Scroll | High frequency noise |
| N + Scroll | Simulation frequency |
| M / , | Show more / fewer layers of the engine |
| F1 / F2 / F3 | Rotate the view / reset the rotation |
| 1 - 5 | Slow motion (1/10 to 1/1000 speed) |
| P | Pause; Right Arrow then advances one frame |
| Page Up / Page Down | Load the previous / next bundled engine |
| [ / ] | Previous / next theme |
| Enter | Reload the current scripts |
| F5 | Show the console (loaded scripts and script errors) |
| Tab | Change screen layout |
| F | Toggle fullscreen |
| Escape | Exit |

The last engine and theme you selected are remembered between sessions.

### Using the RPM hold
The RPM hold holds the engine at a specific speed and measures its power and torque there. Press `H` to enable the hold, then **enable the dyno** with `D` for the hold to take effect. Hold `G` and scroll to change the hold speed, which is shown on the `DYNO. SPEED` gauge.

## Command line

```
engine-sim-app [options] [engine.mr]

  --engine <file>     Engine script to load (same as passing it positionally)
  --theme <file>      Theme script to load
  --data <dir>        Directory containing the assets/ and es/ folders
  --fullscreen        Start in fullscreen mode
  --windowed          Start in windowed mode
  --version           Print the version and exit
```

The data directory is found automatically next to the executable (or inside the macOS app bundle). It can also be set with the `ENGINE_SIM_DATA_ROOT` environment variable.

## Files and folders

| Location | Contents |
| :-- | :-- |
| `assets/main.mr` | Default script, loaded when no engine has been selected yet |
| `assets/engines/` | Bundled engines (piston and rotary) |
| `assets/themes/` | Color and unit themes |
| `es/` | The scripting library (`engine_sim.mr`) and the part library |
| User data folder | `session.ini` (last engine and theme) and `error_log.log`. On Windows `%APPDATA%\EngineSimulator\engine-sim`, on Linux `~/.local/share/EngineSimulator/engine-sim`, on macOS `~/Library/Application Support/EngineSimulator/engine-sim` |

Scripts placed in `assets/engines` or `assets/themes` inside the user data folder also show up when cycling engines and themes, and `import` statements search that `assets` folder too.

## Making your own engines

Engines, vehicles, transmissions and themes are described in a small scripting language. Start with [docs/scripting.md](docs/scripting.md), which covers the structure of an engine script, units, the most important parameters, rotary engines and themes. The bundled engines in `assets/engines` are good starting points.

An engine script ends by selecting what to simulate:

```
public node main {
    run(
        engine: my_engine(),
        transmission: my_transmission(),   // optional
        vehicle: my_vehicle()              // optional
    )
}

main()
```

Scripts can be loaded with `--engine`, by changing `assets/main.mr`, or by pressing Page Up / Page Down in the application. If a script has errors, press F5 to see them.

## Building from source

The project uses CMake (3.16+) and a C++17 compiler. All third-party code is included in `dependencies/` (see [dependencies/README.md](dependencies/README.md)); only SDL2 is needed from the system, or it can be downloaded and linked statically with `-DENGINE_SIM_BUNDLED_SDL=ON`.

### Linux

```
sudo apt install build-essential cmake ninja-build libsdl2-dev libgl-dev   # Debian/Ubuntu
sudo dnf install gcc-c++ cmake ninja-build SDL2-devel mesa-libGL-devel     # Fedora

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/engine-sim-app
```

### macOS

```
brew install cmake ninja sdl2
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
open "build/bin/Engine Simulator.app"
```

### Windows

Install Visual Studio 2022 (with the C++ workload) and CMake, then from a developer command prompt:

```
cmake -S . -B build -DENGINE_SIM_BUNDLED_SDL=ON
cmake --build build --config Release
build\bin\engine-sim-app.exe
```

### Build options

| Option | Default | Description |
| :-- | :-- | :-- |
| `ENGINE_SIM_BUILD_APP` | `ON` | Build the graphical application (turn off for headless tools only) |
| `ENGINE_SIM_BUILD_TESTS` | `ON` | Build the unit tests (uses the system GoogleTest or downloads it) |
| `ENGINE_SIM_BUNDLED_SDL` | `OFF` | Download SDL2 and link it statically |
| `PIRANHA_ENABLED` | `ON` | Scripting support |
| `PIRANHA_REGENERATE_PARSER` | `OFF` | Regenerate the script parser with Flex and Bison |
| `DISCORD_ENABLED` | `OFF` | Discord rich presence (Windows only) |
| `DTV` | `OFF` | Video capture with the Insert key (requires FFmpeg) |

`cmake --install build --prefix <dir>` produces the same layout as the release packages.

### Tests and script validation

```
ctest --test-dir build --output-on-failure
./build/engine-sim-script-check --require-running assets/engines/rotary/two_rotor_13b.mr
```

`engine-sim-script-check` compiles scripts and runs each engine headlessly (starter, then throttle) without opening a window. It is used by CI to make sure every bundled engine still loads and runs. Use `--trace` to print the state of the first cylinder every frame while developing an engine, and `--crank <seconds>` to crank for longer.

## Releases

Pushing a tag such as `v0.2.0` runs the [Release workflow](.github/workflows/release.yml), which builds the Windows, Linux and macOS packages and publishes a GitHub release using the matching section of `CHANGELOG.md` as release notes. Tags containing a hyphen (e.g. `v0.3.0-beta1`) are published as pre-releases. To cut a release:

1. Update `VERSION` in the `project()` call of `CMakeLists.txt` and add a section to `CHANGELOG.md`
2. Commit, then `git tag v<version>` and `git push origin v<version>`

Every push is also built and tested on all three platforms by the [Build workflow](.github/workflows/build.yml).

## Credits

Engine Simulator was created by Ange Yaghi ([YouTube](https://www.youtube.com/c/angethegreat)). The script library additions from the Community Edition are the work of its contributors. Third party components and their licenses are listed in [dependencies/README.md](dependencies/README.md).

## Patreon Supporters

This project was made possible by the generous donations of the following individuals!

### Grease Monkeys

|<!-- -->|<!-- -->|<!-- -->|<!-- -->|<!-- -->|
|-|-|-|-|-|
|Devin@Hondatuningsuite|nut|Devin C Martinez|WelcomeCat|Saints Sasha|
|Ida 8858|Emily|Steelorse |Kruddy|Sgt. Fluff|
|darcuter|FatFluffyFox|Benton1234|Jim C K Flaten|The Zuck|
|Blade Skydancer|Ye' old apple|Hayden Henderson|AlphaX|Lucas Martins Bündchen|
|Jay Dog|damo|IBS-IS-CRAP|Snowy|Noah Greenberg|
|Eisberg|Brendan M.|Alex Layton|Lukas Bartee|Thibaut Dubuisson|
|The Cheeze Ity|JoeJimTom|MichaelB450|Björn|Bartdavy|
|sasha bandelier|Caleb Black|COOKIES|Andrew Cooper|asimo3089|
|Vim Wizard|Kevin Arsenault|Carl Linden|Kele Tappi|Kroklethon|
|labourateur|viperfan7|SlimmyJimmy|Jason Becker|Sascha Kamp|
|ves|Supernalboot |BeamNG|Paul Harrison|Tyler Russell (Nytelife26)|
|nicholas jacobs|DrDotMadness|AVeryPlainTyler|Zach Perez|Paul Schaefer|
|Clay Bauer|CR33DYM0N14|julien nadeau|Patt313|Philip Edwards|
|RegularRuby670|Mateusz Ładosz|FémLol Stúdió|Crazy Yany|Elden|
|Tristan Walker|Matthew McDonald|Jan-Sander Huiting|Mitchell Almstedt|Dylan Lebiedz|
|Name Here|LoganBoi FNAF|Epic Randomness|MrPiThon|mike |
|dung|Alvaro ArroyoZamora|Skinna Godwin|BeppoBarone|レナVA|
|Sabata |Brady Fulham|Powerpuncher |NK10K|Gavin Osowski|
|Orbitstrider|Steven Doyle|Jaksu2696|Toni |Devin Abolins|
### Tuners

|<!-- -->|<!-- -->|<!-- -->|<!-- -->|<!-- -->|
|-|-|-|-|-|
|Boosted Media|Matthew McLennan|Venican|Lyan le Golmuth|Alberto R.|
|BetaToaster|Akira Takemoto|J Anderson|Apolly007|LexLuther|
|xilophor|Robert K|viktor lind|Adrian Kucinski|sarowie .|
|Chris Fischer|Marlod|Chase Hansen|Aidan Szalanski|Andrew Taylor|
|Jason Hwang|Juuso Natunen|Ian Moss|PickleRick |Beljim46|
|RSOFT92|UCD|Sped|OldManJenkins|James Hart|
|Kalle Nilsson|XxBrasta455xX |Colin Sandage|Dakota Mackinnon|Carter Kopp|
|Jakub Kozak|CJ Plessas|Loizeau|Charles Mills|YellowLight|
|Didrik Esbjug|Alessandro Dal Pino|Carter Williams|Robert D|Cadence Plume|
|BLANK|Provenance EMU|Dylan Engler|Nathan Rojas|Cornelius|
|Acid|larsloveslegos|Maxime Desages|GM|BreadForMen|
|Devin Freeman|Lieven RYCKEBOER|Amelia Taylor|Jelle Plukker|sodmo |
|Jimmy Briscoe|Cirithor|Martin .K|DMartland|Lucas Diem|
|Richard Budíček|Jack Sheppeard|Meemen|Anderson Huynh|NPException|
|Mattia Villa|C|AIDAN POWELL|Brenn_the_Otter|Lane Mosier|
|Ceze |oranjest1|Jw|ISON |Mathew Graham|
|MACHINA|John Crowell|Asher Blythe|Cronos Skies|Matt Amott|
|CpTKugelHagel|Simon Krayer|Caleb Bek|Monster Man25|GeneralMoineau|
|EsuKurimu|Caleb Sartin|Jared L.|Hunter Wood|Ben Poole|
|Steven Victoria|Jordan Zondlak|Agelessgod|Christopher Fahs|Jonathan Vincent|
|Dalton Guillot|Simon Stojanovic|Andrew Urbanczyk|deniaL|Tyler Hughes|
|vPam |Justin Kruithof|Curtis C Coomber|Sawyer Clark|Mike Hart|
|Ciro Rancourt|Miles Guo|Rewind |E=mc^2|Keaton Call|
|J.Es|Jeremy B|Chance Hall|Jack Tompkins|Race Sim Studio|
|Quentin ZAOUI|Floyd Henderson|James Haylow|Milkshiekh |Wyatt Todd|
|User 2820|Leon Schutte|CYBERBUG_JR|sebiii|Keegan|
|Victor Cosiuga|Rolly !|Elias Pettersson|Tyson Makovec|Bill McDermott|
|Phontonic|Simon Armstrong|avec |KidozyGAME (Dead)|Stephan Cote|
|Justin Biggerstaff|Jabba Jubba|notD34THNIGHT|Inventor|Wesley Bear|
|Supersonic2510|Pixel|Simon Bernhardt|Bas Vangermeersch|ToyotaCipra|
|kyle crawford|ApatheticWood|Ben Vaughn|Erich Westhoven|Zack Myers|
|Tbjoern|Vetle Høgås|Derek Thom|Aaron Beck||
### Junior Mechanics

|<!-- -->|<!-- -->|<!-- -->|<!-- -->|<!-- -->|
|-|-|-|-|-|
|Karol Szép|Leon Jordan|Nathan Higginson|Patrick F|Samuel Picard|
|Alexander Fritsch|Lucas Scarpi|Jack Humbert|G2Eneko|SweCreations|
|Marius Becker|Cedric Wille|infernap12 |Julian Dinges|Wamuthas|
|Alex Mason|Hawar Karem|Melonenstrauch|Jacek Dębski|Alex Eastman|
|Darren Taing|Po Wang|Giorgio Iannucci|Levis|Eden|
|Alin Chiparatu|Arjun Mandakath|A.M. |Dylan Ryan|Noah Entrekin|
|GT130|Josh D|generic|Henrik Cohrs|Nic Yetter|
|Dan Fredriksen|153AN1MJ|Rasmus|EpicEcho|Kaur Hendrikson|
|Maddox Partridge|L33TIFY_|Zack Fletcher|teiiio|Mike Zaite|
|Evan Sonin|Christopher Zimmerman|PrefacedVase |funtomr|Triton Alabaster|
|appelpie|Julien Ferluc|AnomalousFerret_|Miles Orozco|Spencer Teeter|
|ThatCanadian|Harry Prabowo|Dylan Rogerson|Jaedyn Allen|Zephyr Sefira|
|Alexander Stone|Mason Little|Wojciech Czop|ryzen5 |Kosta Diamantis|
|Karol Stodolak|Tim van der Linde|Loïc Ruttner|jonthefuzz|AsgarK|
|James Morgan|Elijah |1ntl|Tobias Johansson|Mome |
|P|SOPA_|Shingekuro|Sean King|Russell Marsh|
|Alyx Ranas|Naters305 |ChrisakaMrXD |Nic |sean|
|Zach Hagedorn|Jhon lenon|Everett Butts|Kyan|ranger Nation|
|Hiago Oliveira|Texi|MrRhody|Inglorious Bastard|Marty Mitchell|
|Justin Chao|ManuelS|Cornelius Rössing|Pedro Freire|Anthony Stuart|
|Hubba Nubba|Skychii|Joe Underwood|Xander_|Notbigdank|
|Sander D.|Lars Joosten|Danksa|Metrostation |Myles Wommack|
|Derrick Sampson|Corey Hannen|Matteo La Corte|Octothorp Obelus|David Baril|
|Soyuz Kafire|Ivan Coha|BigElbowski|Apolepth|Julian Krad|
|David Soulieres|Eric Huang|Léo Vias|Riccardo Mariani|Vic Viper|
|Shinkaaaa|Mumaransa |Michael Banovsky|Hendrik Voss|Inverted Blackhat|
|skipyC |Tobias Moor|jaky3 .|Clément LEGRAND|Ian C. Simpson|
|Challier|Jan Przemysław Drabik|Dsand23|Smooth DLX|The German Dude|
|CrazyEagle |Jordon Goodman|HenryWithaG .|Oscar Krula|Brayden Moore|
|Steven|Nall Wolfert|papajonk|Andrew|Ben Kingston|
|Julian Vogl|Maxime Lubrano|MrMekouil|Doudimme|Jacob Hultberg|
|Nolan Orloff|Mike|tobi9899 |Danila Frolkin|Xecotcovach|
|Aj|Carcar404|John Martin|Dominik Greinert|Lukas Stadler|
|Oliver Yang|sonax51|Marcel Kliment|Chris|David Rush|
|LethalVenom13|Dave Osterhoff|Anto1709|Ben|Morgan Munroe|
|Ivor Forrest|Sam Hopkins|Atte |Dax |William Bergström|
|homelessmeme|Thanleft|Zaxerg |Robeloox|Maximilian-Lukas Marz|
|Morgn|Seth Monteleone|playfulmean videos|Lanimations LA|Bram G|
|Benoit Fournier|Bernar Lepiller|Nicolas Baur|the |Snekers|
|Darkmount|Tobiasz Michalik|Aidas Ri|Daniel Postler|Skim_Beeblez|
|Impetus|Thunderbird324|Fred Joss|Krzysztof Radowski|Azerrty|
|Harrison Speck|Matt Baker|BigLynch|Markus Pelto|IMBIBE|
|James L Plummer|Rose Giles|Jonas Brekka|HASTRX|Lepoucehumain|
|Naomi Humin|qkrrudgks|Johann Gross|Janis Knappich|WhatTheDuck|
|테루|Glimple Bort|Jacob Tudisco|Tanner|Julian kaspi|
|nathan gould|Randal Rainis Kruus|Beppierre|Craig Martin|Thomas Bukovsky|
|Colaxe|Robert Oram|Matsuy15 L|Kacpe|Alex Sedlic|
|Mark Benson|Mhenn!|Anders Nelson|Dingus|Rustle|
|Marco Schulz|stratum |brochier gabriel|Thomas|brody of hillcountry|
|Thomas Afford|Brody Blaskie|Martien Gaming|Adrien MC|William A Grubbs|
|Trevo Ph.D.|Donovan Gibson|Polish R3t4rd|Keith Price|LAWL CAKE|
|Rhien Schultz|FireThrow13|Seraphim|Titus Standing|Matt Miklos|
|B Dub|Jonathan Ekman|Al Pomeroy|Vestii|Wil|
|adrian|Airatise|TJ Sinkoski|Shotts SilverStone|Reagan Carbaugh|
|WarAestheticsRebooted|Aidan Case|Casey Bryant Goodwin|Konrad|Adam Larcher|
|Kazar Xin Xiao|Riccardo Marcaccio|William S.|Francis Filion|Loïc |
|Kenny Deane|Blackspots|mike |MXT|Joshua Gibson|
|milky boi|Hagen|gunmaster929 |jgvan |Benny 282|
|Sean Wehner|Christian Poole|Ethan|Tsukiyama Shuu|Ooof_uhhh_haah|
|sano ken ch|Diego Martinez|Chuck|GalaxyFrogs|TheGeForce |
|Chriphost|Carthage|Greg L|Chipskate|Muhammed Mehmood|
|Hamilton Sjoberg|Amina Moh|vSiiFT|Jeremy Wren|Esteban Acosta|
|John A Ullenberg|Michael Morozov|Andrew Webberley|Nathaniel Lim|Aaron Ksicinski|
|Apocalypt|Josh batuzich|Ed|Hunter |Gene Brockoff|
|Redheadspellslinger.|Pablo Magariños|Nilz|Jose Manuel Silva Calvo|AJ|
|Ethan Wille |Aurora|Derek Shunia|Jan|Nope Mircea|
|Giancarlo Cestari|Tanner Edge|brad.|Connor Merrick|Martin Scholer|
|Deppy|Dan Smith|Tyson |Jac Comeau|Itemfinder |
|Tischer Games|Pedro Henrique|BeenWashedUp|martin wolff|Kurt Houben|
|Thomas Onslow|Brendan Puglisi|Kai Anquetil|Rudolph Ignatenko|CloudHackIX|
|Zach Carreau|Jonathan Vanderlyn|Krobivnov|ienergy|Leifster|
|Mikael Kaaronen|Glen|H.Helsing|ange|The Nobles|
|Johnathan Johnson|Juha Merentie|Jim Fares|Tom Marshall|Superferrariman|
|Zakary Zisa|JustTy|晟道 杜|Dnialibr Williams|Takumi Fujiwara|
|Koen van Hal|Jonathan Hill|Marco Siciliano|Kevindosenfutter|Angry Prawn|
|Natharic 67|Rafael Monteiro|Jacob Ashline|ChironTheFloof|Caleb Dauphinee|
|Tony |Zac L|AlainMoto FPV|eirik johan johnsen|Elderet|
|Miles Longmore|lemon head|Viccy|Casey|Kajetan Cupa|
|Conejero00|Bill Gricko|A cow wearing a turban|Danni Nowicki|Udo Schmidt|
|Tyler Swords|Constellation Gaming|Manimo|valentine|Jules Schattenberg|
|Brandon Crotts|Philipp Popetschnigg|Tiziano Della Fazia|goodgamer1109|Joshua Thomas|
|Jeff Testa|Avery Snyder|Josh Kern|Triptagram|Bayon Antoine|
|Iván Juárez Núñez|Amery Martinat|ElArGee|Cory Green|lucas Di lorenzo|
|Caleb Sandersier|||||
