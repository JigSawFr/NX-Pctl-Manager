# Changelog

## 1.0.0 (2026-10-11)


### ⚠ BREAKING CHANGES

* new name and SD-card folder (sd:/switch/playguard/).

### 🚀 Features

* a summary per period and games to rediscover in Activity ([#81](https://github.com/JigSawFr/PlayGuard/issues/81)) ([3543af7](https://github.com/JigSawFr/PlayGuard/commit/3543af75735a9299ada5fb36f18dc789d02801ca))
* About tab, What's new after updates, and ways to support PlayGuard ([#34](https://github.com/JigSawFr/PlayGuard/issues/34)) ([a7a4fb3](https://github.com/JigSawFr/PlayGuard/commit/a7a4fb361b6c8770bfb9ea57e622d13fc24285c5))
* Activity day by day, deleted games' totals and play time per account ([#21](https://github.com/JigSawFr/PlayGuard/issues/21)) ([33f2153](https://github.com/JigSawFr/PlayGuard/commit/33f215358e34bbd77310d387737d80c6786f6c3e))
* Activity tab with play time per game ([0a6a556](https://github.com/JigSawFr/PlayGuard/commit/0a6a556333afdda93ca5c75cb826e6ebe4d5f4c8))
* ask for the PIN before a change, or to open PlayGuard ([#19](https://github.com/JigSawFr/PlayGuard/issues/19)) ([bb91242](https://github.com/JigSawFr/PlayGuard/commit/bb91242f4ee15e7189b3f58d44dbafc0d46954bc))
* back up and restore the parental-control settings ([0a6a556](https://github.com/JigSawFr/PlayGuard/commit/0a6a556333afdda93ca5c75cb826e6ebe4d5f4c8))
* bedtime alarm display and opt-in advanced actions (time's up alarm, pause / resume) ([4a00686](https://github.com/JigSawFr/PlayGuard/commit/4a00686d376fc9bd8facadbb15f5699ffb85ac02))
* change the bedtime alarm, checked against what the console reports ([#44](https://github.com/JigSawFr/PlayGuard/issues/44)) ([dd86342](https://github.com/JigSawFr/PlayGuard/commit/dd86342ace23781d518ca2ec03da3ab584e15ab0))
* console lock — one switch for a 0-minute limit every day ([#28](https://github.com/JigSawFr/PlayGuard/issues/28)) ([ec78f4f](https://github.com/JigSawFr/PlayGuard/commit/ec78f4fe557b97065af00e4ea7b9d3ac37bfc290))
* **dev builds:** the build list at once, kept and refreshed on demand; artifacts download through a GitHub App; network tasks no longer wait behind the play log ([#48](https://github.com/JigSawFr/PlayGuard/issues/48)) ([18144ca](https://github.com/JigSawFr/PlayGuard/commit/18144caae721d56f522a708dc60eff3ee053f33c))
* developer tool to compare the play-timer block, exemption list in the diagnostic report ([#22](https://github.com/JigSawFr/PlayGuard/issues/22)) ([8e8841d](https://github.com/JigSawFr/PlayGuard/commit/8e8841deed56998425f9655c10e4d820699e68cd))
* development builds from the build artifacts, signed in to GitHub with a QR code; no more pre-releases ([#40](https://github.com/JigSawFr/PlayGuard/issues/40)) ([a2c6ce6](https://github.com/JigSawFr/PlayGuard/commit/a2c6ce6a5ec810aac2d40c06568b472feb12f76d))
* diagnostic report in sd:/switch/playguard/logs/ (never contains the PIN) ([4a00686](https://github.com/JigSawFr/PlayGuard/commit/4a00686d376fc9bd8facadbb15f5699ffb85ac02))
* export the Activity figures to the SD card as CSV, JSON, XLSX or PDF ([0a6a556](https://github.com/JigSawFr/PlayGuard/commit/0a6a556333afdda93ca5c75cb826e6ebe4d5f4c8))
* extra time today from the Play timer tab ([0a6a556](https://github.com/JigSawFr/PlayGuard/commit/0a6a556333afdda93ca5c75cb826e6ebe4d5f4c8))
* firmware 23.0.1 / Atmosphère 1.12.0 support with firmware and Atmosphère detection ([4a00686](https://github.com/JigSawFr/PlayGuard/commit/4a00686d376fc9bd8facadbb15f5699ffb85ac02))
* French translation, language and theme preferences, readable error messages ([4a00686](https://github.com/JigSawFr/PlayGuard/commit/4a00686d376fc9bd8facadbb15f5699ffb85ac02))
* install a development build in place from the developer tools ([#39](https://github.com/JigSawFr/PlayGuard/issues/39)) ([9af654e](https://github.com/JigSawFr/PlayGuard/commit/9af654e0afcac2ccebf44f73fd54ffb74883a99b))
* lock parental controls again right after a write ([4a00686](https://github.com/JigSawFr/PlayGuard/commit/4a00686d376fc9bd8facadbb15f5699ffb85ac02))
* network clock from ~50 public NTP servers, measured against 3 servers (adapted from anbingxi's fork) ([4a00686](https://github.com/JigSawFr/PlayGuard/commit/4a00686d376fc9bd8facadbb15f5699ffb85ac02))
* new PlayGuard logo and store art ([f576890](https://github.com/JigSawFr/PlayGuard/commit/f576890605c4709a923043fe7ad2ef4f2123e538))
* onboarding, applet warning, Activity totals, week view and i18n polish ([#14](https://github.com/JigSawFr/PlayGuard/issues/14)) ([17bc3f1](https://github.com/JigSawFr/PlayGuard/commit/17bc3f1f97d1b2740d2514bd71c30a90015c431b))
* per-day editor with no-limit days, Monday–Friday / weekend presets, copy a day and discard changes ([4a00686](https://github.com/JigSawFr/PlayGuard/commit/4a00686d376fc9bd8facadbb15f5699ffb85ac02))
* play-time limit profiles saved on the SD card ([4a00686](https://github.com/JigSawFr/PlayGuard/commit/4a00686d376fc9bd8facadbb15f5699ffb85ac02))
* PlayGuard in every console language ([#25](https://github.com/JigSawFr/PlayGuard/issues/25)) ([8ff5b0c](https://github.com/JigSawFr/PlayGuard/commit/8ff5b0c19a358b19bfbb76d15b3d67043cfce236))
* preferences — start tab, extra-time amounts, remembered Activity choices, daily update check, start-up clock check, backups kept ([#17](https://github.com/JigSawFr/PlayGuard/issues/17)) ([cfb9d44](https://github.com/JigSawFr/PlayGuard/commit/cfb9d446504575a0c2b8c014f4e033ba7e310a43))
* profiles with any name, edited and renamed without applying; backups keep the alarm, the rating body and the raw play-timer block ([#18](https://github.com/JigSawFr/PlayGuard/issues/18)) ([392499d](https://github.com/JigSawFr/PlayGuard/commit/392499dfbb48d9badf546f9ab3c80cfc2d744f06))
* READ_ONLY build "PlayGuard Diagnostics" that cannot change anything ([4a00686](https://github.com/JigSawFr/PlayGuard/commit/4a00686d376fc9bd8facadbb15f5699ffb85ac02))
* record the play timer every 30 s, explain the clock sync setting, document how the parental controls work ([#46](https://github.com/JigSawFr/PlayGuard/issues/46)) ([884f94f](https://github.com/JigSawFr/PlayGuard/commit/884f94f73105e6e98fa46f09aae77663b93235c4))
* recovery sysmodule and screen for a forgotten PIN that blocks the console ([#27](https://github.com/JigSawFr/PlayGuard/issues/27)) ([39be7fc](https://github.com/JigSawFr/PlayGuard/commit/39be7fc2cfc4a6ca49cef0f67fc9f93933d37662))
* restrictions — what a preset restricts before choosing it, the rating organisation editable and restored from backups ([#20](https://github.com/JigSawFr/PlayGuard/issues/20)) ([10bfe9d](https://github.com/JigSawFr/PlayGuard/commit/10bfe9dfd6dbf19183128934bf49e18e5a2fefcd))
* restrictions tab (level, age rating, social-media posting, communication, VR mode) ([4a00686](https://github.com/JigSawFr/PlayGuard/commit/4a00686d376fc9bd8facadbb15f5699ffb85ac02))
* say that PlayGuard's time over a game counts, and leave it out of the Activity tab ([#37](https://github.com/JigSawFr/PlayGuard/issues/37)) ([b401170](https://github.com/JigSawFr/PlayGuard/commit/b40117043789643861cb666c7b74f7199c64093d))
* say when the console is not counting play time, what it does at the limit, and what changed outside PlayGuard ([#87](https://github.com/JigSawFr/PlayGuard/issues/87)) ([2ec32af](https://github.com/JigSawFr/PlayGuard/commit/2ec32af5d716d2cda7a221085767d9d711f9b50a))
* send a diagnostic report online, with the debug files, as a link and QR codes ([#38](https://github.com/JigSawFr/PlayGuard/issues/38)) ([21f38d4](https://github.com/JigSawFr/PlayGuard/commit/21f38d4a693d92b14e63927bbfe535581947131c))
* show the PIN in the Security tab ([0a6a556](https://github.com/JigSawFr/PlayGuard/commit/0a6a556333afdda93ca5c75cb826e6ebe4d5f4c8))
* single build with runtime read-only, developer mode and firmware screen ([#8](https://github.com/JigSawFr/PlayGuard/issues/8)) ([111afeb](https://github.com/JigSawFr/PlayGuard/commit/111afebdaa53d784435e10ffeea41b3ffa789d8a))
* tabbed interface (Overview with a play-time gauge, Play timer, Restrictions, Network clock, Companion app, PIN & security, Tools & about) ([4a00686](https://github.com/JigSawFr/PlayGuard/commit/4a00686d376fc9bd8facadbb15f5699ffb85ac02))
* the Activity tab opens at once on its last figures, kept on the SD card, and refreshes them quietly ([#42](https://github.com/JigSawFr/PlayGuard/issues/42)) ([5f38ec0](https://github.com/JigSawFr/PlayGuard/commit/5f38ec07c1d012451485da8f36a3cf8e3809168f))
* the console's live clock and time zone for "today", midnight while open, pending and automatic extra-time restore ([#16](https://github.com/JigSawFr/PlayGuard/issues/16)) ([32ee158](https://github.com/JigSawFr/PlayGuard/commit/32ee15858aaaa2c779e3d20469a32db81f572fb6))
* the play-timer recording says what happened, in an event column ([#66](https://github.com/JigSawFr/PlayGuard/issues/66)) ([e535ca9](https://github.com/JigSawFr/PlayGuard/commit/e535ca930847a26b1c12a771d1e7261e5bca7af3))
* UI/UX overhaul — editable week chart, first steps, change history with undo, per-account activity, Preferences tab ([#24](https://github.com/JigSawFr/PlayGuard/issues/24)) ([3c0b950](https://github.com/JigSawFr/PlayGuard/commit/3c0b950efb975ce5ef3abbf3fca0429faa727171))
* UI/UX overhaul, play-timer profiles and extra time, console information and PlayGuard naming ([#6](https://github.com/JigSawFr/PlayGuard/issues/6)) ([97af3c8](https://github.com/JigSawFr/PlayGuard/commit/97af3c8f447b6a1943c181f81544181c73670aee))
* **upload:** send reports to bpa.st, or a secret gist when signed in to GitHub ([#49](https://github.com/JigSawFr/PlayGuard/issues/49)) ([a16524e](https://github.com/JigSawFr/PlayGuard/commit/a16524eb8f6b12f736f1304db2cc868f06b3518d))
* warn while the "time's up" alarm is off, and compare PlayGuard with replacement sysmodules ([#30](https://github.com/JigSawFr/PlayGuard/issues/30)) ([a0dd1da](https://github.com/JigSawFr/PlayGuard/commit/a0dd1da1ba46a0df9ed2dabfac4580c34ac72308))


### 🐞 Bug Fixes

* a failed read or save no longer loses extra time, the history or a block the timer still counts ([#72](https://github.com/JigSawFr/PlayGuard/issues/72)) ([360507f](https://github.com/JigSawFr/PlayGuard/commit/360507fff070e6a93bd5df5a09325875a8a429b7))
* a hand-written rescue report, a damaged config or an unlocked exit no longer get past the PIN ([#70](https://github.com/JigSawFr/PlayGuard/issues/70)) ([5d2376c](https://github.com/JigSawFr/PlayGuard/commit/5d2376c1b2cb6ba8cff40e15a9d5292692690a97))
* an uncaught exception crashes PlayGuard, not hbmenu after it ([#68](https://github.com/JigSawFr/PlayGuard/issues/68)) ([607ce67](https://github.com/JigSawFr/PlayGuard/commit/607ce67fcbeb3041c4ff159fa71020015752acd8))
* every D-pad press moves one line, nothing hidden at the ends; "..." on the console font ([#35](https://github.com/JigSawFr/PlayGuard/issues/35)) ([33a57a4](https://github.com/JigSawFr/PlayGuard/commit/33a57a4ad9555ea27117f58aea94f8e4d1806e78))
* findings of a full review — two crashes, the play-timer gate, relock safety, console lock, rescue delete ([#33](https://github.com/JigSawFr/PlayGuard/issues/33)) ([edbf386](https://github.com/JigSawFr/PlayGuard/commit/edbf386c026a63fd209b213e25b82c3719ae275b))
* harden the PIN: Show the PIN always asks, "open" locks again after time away ([#83](https://github.com/JigSawFr/PlayGuard/issues/83)) ([9807925](https://github.com/JigSawFr/PlayGuard/commit/980792579929e666f076ffa309390067a0d87cb5))
* keep damaged settings and history, check the play-timer block, gate firmware 23.x ([#84](https://github.com/JigSawFr/PlayGuard/issues/84)) ([4ccbd20](https://github.com/JigSawFr/PlayGuard/commit/4ccbd204f0c37516f27cc36c58ea4825dfa8f78b))
* keep the play-timer fields PlayGuard does not decode, relock after an unverified unlock, resilient preferences ([#10](https://github.com/JigSawFr/PlayGuard/issues/10)) ([cde7aef](https://github.com/JigSawFr/PlayGuard/commit/cde7aef6296bf7462ce0157c9a09a3c3cd156700))
* label the week charts, mark over-limit and unsaved days, unchecked extra-time list, download progress ([#76](https://github.com/JigSawFr/PlayGuard/issues/76)) ([101f81f](https://github.com/JigSawFr/PlayGuard/commit/101f81f0ec14976b98af84c375e7efbc01fc2afb))
* Latin font for every label on the console, no cross-fade between screens ([#36](https://github.com/JigSawFr/PlayGuard/issues/36)) ([2c45307](https://github.com/JigSawFr/PlayGuard/commit/2c45307031c242775f48406441594f3663e8f681))
* open the pctl:a session per action and release it right away (Atmosphère crashes on 22.5) ([4a00686](https://github.com/JigSawFr/PlayGuard/commit/4a00686d376fc9bd8facadbb15f5699ffb85ac02))
* play-timer write gate moved into the service layer, also covering "Remove the limit" ([4a00686](https://github.com/JigSawFr/PlayGuard/commit/4a00686d376fc9bd8facadbb15f5699ffb85ac02))
* sign in to GitHub with the recreated PlayGuard OAuth app ([#63](https://github.com/JigSawFr/PlayGuard/issues/63)) ([5cd6326](https://github.com/JigSawFr/PlayGuard/commit/5cd6326f27f23fcee3d0b0e6424b97b980069e30))
* the first steps' clock check no longer hangs on its spinner ([#43](https://github.com/JigSawFr/PlayGuard/issues/43)) ([81967c3](https://github.com/JigSawFr/PlayGuard/commit/81967c39dc53b83b4afd86b0122e232f9dfeb321))
* the PIN lock screen says what the PIN screen answered ([#55](https://github.com/JigSawFr/PlayGuard/issues/55)) ([5352990](https://github.com/JigSawFr/PlayGuard/commit/5352990d378e21aeef13a9532bd0dd843053bb65))
* the right PIN opens PlayGuard from the lock screen ([#65](https://github.com/JigSawFr/PlayGuard/issues/65)) ([83db34f](https://github.com/JigSawFr/PlayGuard/commit/83db34f5d2ec280f2bab322189767a9a2e6d404b))
* UI feedback and consistency (unlocked title, B cancels, danger buttons, clock and update checks, plain app errors) ([#11](https://github.com/JigSawFr/PlayGuard/issues/11)) ([0251ce8](https://github.com/JigSawFr/PlayGuard/commit/0251ce868cbc8ad2620d93d21cc309edcf176c79))
* **upload:** send the report as multipart and show dpaste's real error ([#45](https://github.com/JigSawFr/PlayGuard/issues/45)) ([d1889c7](https://github.com/JigSawFr/PlayGuard/commit/d1889c7a08e4ab67ca2518e355177975567b4451))
* verify the temporary unlock before writing ([4a00686](https://github.com/JigSawFr/PlayGuard/commit/4a00686d376fc9bd8facadbb15f5699ffb85ac02))
* warn about the recovery sysmodule, confirm loosened restrictions, say what to do after a system error, accept any script in profile names ([#75](https://github.com/JigSawFr/PlayGuard/issues/75)) ([7d4ae43](https://github.com/JigSawFr/PlayGuard/commit/7d4ae43a677dc92b153c2853ff9f163695412a79))


### ✨ Polish

* game names and icons kept between runs, compressed titles read, quitting no longer waits for a play-data read ([#85](https://github.com/JigSawFr/PlayGuard/issues/85)) ([dc6f467](https://github.com/JigSawFr/PlayGuard/commit/dc6f467d813e1c916b50d72e6c5d6484b682e367))
* one pctl session per Overview refresh, clocks read every 10 s, per-run caches ([#13](https://github.com/JigSawFr/PlayGuard/issues/13)) ([452e639](https://github.com/JigSawFr/PlayGuard/commit/452e639cb06243bbe858d1d0e340f4830c87438f))
* rename the project to PlayGuard, JigSawFr as author ([6aff9e1](https://github.com/JigSawFr/PlayGuard/commit/6aff9e1571ba88eacf1be1dfb6ba81afdafb1c4b))
* split ui.cpp by concern, one game sort, fsync before rename, strict config lists ([#77](https://github.com/JigSawFr/PlayGuard/issues/77)) ([08a9c61](https://github.com/JigSawFr/PlayGuard/commit/08a9c61b49e6f9d23f414cedc9b9b2bd2f6e4672))
* stop leaving PlayGuard's own time out of the Activity tab ([#64](https://github.com/JigSawFr/PlayGuard/issues/64)) ([135dbbe](https://github.com/JigSawFr/PlayGuard/commit/135dbbeb5c0b0d3269432d47b965b3a5c349b4cf))
* the Activity list updates in place, builds its first 50 rows, decodes each icon once; background tasks end before the app ([#74](https://github.com/JigSawFr/PlayGuard/issues/74)) ([7b63af0](https://github.com/JigSawFr/PlayGuard/commit/7b63af035962b646de3d930313ce1f4e14adb7b8))
* the UI's last console calls behind core/platform.h, simulated on the desktop ([#23](https://github.com/JigSawFr/PlayGuard/issues/23)) ([b2c362d](https://github.com/JigSawFr/PlayGuard/commit/b2c362d42f7df52467e0ec1b4c5b7de08160108b))


### 🧰 Other

* a code of conduct, a support page, translation and console-report forms, labels kept in a file, a social preview image ([#82](https://github.com/JigSawFr/PlayGuard/issues/82)) ([9ac96c0](https://github.com/JigSawFr/PlayGuard/commit/9ac96c04c1717a48e13bae4d664fe70ff8b2c9f6))
* draw with deko3d on the Switch (.nro 9.4 MB -&gt; 3.7 MB) ([#12](https://github.com/JigSawFr/PlayGuard/issues/12)) ([bd345c3](https://github.com/JigSawFr/PlayGuard/commit/bd345c3172d30835850aa0ad7dc9d801e269be19))
* PlayGuard's own warnings on (errors in CI), the last English strings translated, docs caught up ([#71](https://github.com/JigSawFr/PlayGuard/issues/71)) ([a444e5b](https://github.com/JigSawFr/PlayGuard/commit/a444e5bf7835f60e54b866678ed2f830c82fa1a9))
* release zip with the sphaira GitHub entry, raw .nro and .zip release assets ([4a00686](https://github.com/JigSawFr/PlayGuard/commit/4a00686d376fc9bd8facadbb15f5699ffb85ac02))
* restore the Switch CPU flags, ccache and a pinned toolchain in CI, sanitized host tests ([#9](https://github.com/JigSawFr/PlayGuard/issues/9)) ([84ca61a](https://github.com/JigSawFr/PlayGuard/commit/84ca61a95d22ec5640f1b24d86ba739abebbe34a))


### 🧪 Tests

* CHECK() instead of assert() in the host suites, clang and coverage in CI, reproducible zips ([#78](https://github.com/JigSawFr/PlayGuard/issues/78)) ([c9cfcbb](https://github.com/JigSawFr/PlayGuard/commit/c9cfcbb7d53dbf8862d90e8bd8b5a5741a6c70f1))
* desktop smoke looks for the PlayGuard window ([85accf9](https://github.com/JigSawFr/PlayGuard/commit/85accf940f9393d1765f575ad9ff5c140f820aec))
* draw the footer clock at the console's time in the visual references, not a black box ([#80](https://github.com/JigSawFr/PlayGuard/issues/80)) ([f9862e6](https://github.com/JigSawFr/PlayGuard/commit/f9862e62bd72d889fbd7819a4b65802971a8b130))
* host tests for the PIN lock, console lock and history undo decisions ([#73](https://github.com/JigSawFr/PlayGuard/issues/73)) ([b9c619b](https://github.com/JigSawFr/PlayGuard/commit/b9c619b5a6740499bc92885822a6b3838c0ab6cc))
* one console-free core for the console and the simulator, failure injection, play-timer decisions under test ([#15](https://github.com/JigSawFr/PlayGuard/issues/15)) ([c2b5c9e](https://github.com/JigSawFr/PlayGuard/commit/c2b5c9e09640762aa6c17cf626fef0817d9f4141))
* unit tests of the C service layer, desktop build with a simulated console, headless smoke test, resource checks ([4a00686](https://github.com/JigSawFr/PlayGuard/commit/4a00686d376fc9bd8facadbb15f5699ffb85ac02))


### 📚 Documentation

* a limit written by PlayGuard is enforced on the console ([#53](https://github.com/JigSawFr/PlayGuard/issues/53)) ([c10c638](https://github.com/JigSawFr/PlayGuard/commit/c10c638cb7a9d47cc81b545d947bee16867c4205))
* a roadmap, and how PlayGuard compares with Nintendo's companion app ([#50](https://github.com/JigSawFr/PlayGuard/issues/50)) ([2482082](https://github.com/JigSawFr/PlayGuard/commit/248208237ab30d7d4d199de6052b41c1b76cdd40))
* emuMMC/sysMMC note, a decode call while linked, honest compatibility FAQ, specific error hints, corrected texts ([#88](https://github.com/JigSawFr/PlayGuard/issues/88)) ([0fdacc0](https://github.com/JigSawFr/PlayGuard/commit/0fdacc0581fc4d27e410cfb982fed6e62581e224))
* every key of config.json, its values and default ([#67](https://github.com/JigSawFr/PlayGuard/issues/67)) ([751c4f8](https://github.com/JigSawFr/PlayGuard/commit/751c4f8e1476226b1cb45ed1533dfb5b240c3482))
* explain conventional commits and squash merges for releases ([4a00686](https://github.com/JigSawFr/PlayGuard/commit/4a00686d376fc9bd8facadbb15f5699ffb85ac02))
* refresh the README screenshots, list every file PlayGuard writes ([#41](https://github.com/JigSawFr/PlayGuard/issues/41)) ([7f71944](https://github.com/JigSawFr/PlayGuard/commit/7f71944356c244ee874897fa43d1f1bc06fcc2c4))
* reorganise the README for readers, move build docs to CONTRIBUTING.md ([#31](https://github.com/JigSawFr/PlayGuard/issues/31)) ([d474430](https://github.com/JigSawFr/PlayGuard/commit/d47443023dd17ba829cac29b677d1e32ece9c75b))
* roadmap horizon mentions iPhone Live Activities fed by the MQTT bridge ([#79](https://github.com/JigSawFr/PlayGuard/issues/79)) ([de32116](https://github.com/JigSawFr/PlayGuard/commit/de32116bbb58d03dd8dc5020d6d19d3cfb297464))
* say how AI assistants were used to build PlayGuard ([#52](https://github.com/JigSawFr/PlayGuard/issues/52)) ([2c886b8](https://github.com/JigSawFr/PlayGuard/commit/2c886b8af5a6e9021665c48b3aa12d86e278ac49))
* what to do when locked out (second-hand console, forgotten PIN) ([#26](https://github.com/JigSawFr/PlayGuard/issues/26)) ([ee19595](https://github.com/JigSawFr/PlayGuard/commit/ee195955060348ffe50f8587ec28fe37537d6786))
* why PlayGuard exists, and the horizon of a bridge to MQTT and Home Assistant ([#51](https://github.com/JigSawFr/PlayGuard/issues/51)) ([a3146a3](https://github.com/JigSawFr/PlayGuard/commit/a3146a3c6d25e349ba18331fb901cd567efe7979))

## Upstream history — Pctl Manager by Taylor

PlayGuard is a fork of Pctl Manager. The entries below are Taylor's releases, kept for reference.

### Pctl Manager v3.0.0

UI rewrite onto [borealis](https://github.com/xfangfang/borealis) — Horizon-system-style graphical UI replaces the v2 text console. **No functional changes** — every action behaves the same.

- App display name is now **"Pctl Manager"** (repo name `NX-Pctl-Manager` unchanged).
- Non-CFW boot shows a dedicated error screen (was a silent fall-through in v2).
- Build switched to CMake; `make` / `./run.sh` interfaces unchanged.
- `.nro` grew 265 KB → ~8.2 MB (borealis library + assets).

Tested on fw 22.1.0 / Atmosphère 1.11.1. `make PROBE=1` still adds the diagnostic dump cell.

### Pctl Manager v2.0.0

First public release. CFW (Atmosphère) only; tested on fw 22.1.0 / Atmosphère 1.11.1.

- **Configure the daily play-time limit offline** — one for all days, per-day limits, or off. When the timer is active, writes turn parental controls off temporarily (the app reads the PIN automatically) so the write doesn't destabilise Atmosphère.
- Set / change PIN via the system passcode applet.
- Delete all parental controls — also the recovery path when the PIN is forgotten.
- Unlink the Nintendo Switch Parental Controls companion phone app.
- View status: safety level, PIN length, restrictions, play-timer state.

`make PROBE=1` adds a read-only "Dump current config" diagnostic cell (off in release builds).
