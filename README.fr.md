# PlayGuard

![PlayGuard](images/store/banner.png)

[![build](https://github.com/JigSawFr/PlayGuard/actions/workflows/build.yml/badge.svg)](https://github.com/JigSawFr/PlayGuard/actions/workflows/build.yml)
[![dernière version](https://img.shields.io/github/v/release/JigSawFr/PlayGuard)](https://github.com/JigSawFr/PlayGuard/releases/latest)
[![téléchargements](https://img.shields.io/github/downloads/JigSawFr/PlayGuard/total)](https://github.com/JigSawFr/PlayGuard/releases)
[![licence : GPLv3](https://img.shields.io/badge/licence-GPLv3-blue.svg)](LICENSE)

[English](README.md) · **Français**

**Le contrôle parental de la Nintendo Switch, directement sur la console — sans application mobile, sans compte Nintendo, sans Internet.**

PlayGuard est un homebrew qui ramène sur la console, hors ligne, les réglages de l'application mobile [Contrôle parental Nintendo Switch](https://apps.apple.com/fr/app/contr%C3%B4le-parental-nintendo-sw/id1190074407) : limite quotidienne de temps de jeu, restrictions, code PIN, horloge réseau, activité de jeu — et un moyen de reprendre la main quand la console est bloquée. Ce qu'il couvre de l'application mobile, et ce qui manque encore : [docs/companion-app.md](docs/companion-app.md) (en anglais).

![Vue d'ensemble](images/screenshots/dashboard_fr_dark.png)

> [!WARNING]
> **Nécessite un firmware personnalisé (Atmosphère).** PlayGuard dialogue avec le service système restreint `pctl` : il ne fonctionne que sur une console modifiée. Il ne contourne aucune vérification de compte ou en ligne, et certaines des commandes utilisées sont des commandes `*ForDebug`. **À utiliser à vos risques.**

## Sommaire

- [Pourquoi PlayGuard](#pourquoi-playguard)
- [Points forts](#points-forts)
- [Compatibilité](#compatibilité)
- [Installation](#installation)
- [Premiers pas](#premiers-pas)
- [Les fonctions, onglet par onglet](#les-fonctions-onglet-par-onglet)
- [Sûr par conception](#sûr-par-conception)
- [Ce qui compte comme temps de jeu](#ce-qui-compte-comme-temps-de-jeu)
- [Console bloquée ?](#console-bloquée--console-doccasion-code-pin-oublié)
- [PlayGuard ou un sysmodule de remplacement ?](#playguard-ou-un-sysmodule-de-remplacement-)
- [Signaler un bug](#signaler-un-bug)
- [Contribuer](#contribuer)
- [Licence et remerciements](#licence-et-remerciements)

## Pourquoi PlayGuard

Les enfants aiment les jeux vidéo, et les jeux vidéo, ce sont des écrans : les limiter fait partie de prendre soin d'eux. Sur une Switch d'origine, l'application mobile de Nintendo s'en chargeait. En passant la console sous firmware personnalisé, le champ des possibles s'ouvre — mais le contrôle parental saute, car l'application mobile n'atteint plus la console. Savoir combien de temps ils ont joué, fixer une limite, faire arrêter le jeu sans bataille ni éternel « encore cinq minutes » devenait compliqué.

Les rares outils existants ne faisaient pas l'affaire : un suivi d'activité peu maintenu et peu détaillé, un contrôle parental peu développé et inabouti. Je ne voulais pas non plus d'un remplacement maison, ni d'un sysmodule qui tourne en permanence en arrière-plan. L'objectif : **réutiliser au maximum le contrôle parental de la console**, avec tout ce qu'il sait déjà faire — la limite, le code PIN, les avertissements, la suspension — et ramener ses réglages sur la console. C'est PlayGuard.

Piloter le contrôle parental de la console ouvre aussi la porte à bien plus : remonter le temps de jeu vers un serveur à la maison, recevoir des ordres, Home Assistant, des automatisations. Ce vers quoi cela pourrait aller est dans [l'horizon de la feuille de route](ROADMAP.md#horizon) (en anglais).

## Points forts

- ⏱️ **Limite quotidienne de temps de jeu** — la même tous les jours ou une par jour, modifiée sur un graphique de la semaine ; **profils** enregistrés (*Semaine d'école*, *Vacances d'été*…) ; **temps en plus aujourd'hui** et **plus de jeu aujourd'hui** en un appui.
- 📊 **Activité** — temps par jeu aujourd'hui, sur 7 jours et depuis toujours, par compte utilisateur, avec graphiques, un résumé par période, des jeux à redécouvrir et **export** en CSV, JSON, XLSX ou PDF.
- 🔒 **Restrictions et code PIN** — niveau de restriction, classification par âge et organisme de classification, définir / afficher le code PIN, déverrouiller temporairement, **verrou de console** en un interrupteur, et un code PIN optionnel pour ouvrir PlayGuard lui-même.
- 🕒 **Horloge réseau** — mesure sur des serveurs NTP publics et réglage, pour que le minuteur compte juste sur une console qui n'atteint jamais Nintendo.
- 📱 **Application mobile** — voir si elle est associée et la **dissocier**, même sur une console d'occasion.
- ↩️ **Historique, sauvegardes et retour arrière** — chaque modification faite par PlayGuard est consignée et peut être annulée ; les réglages peuvent être sauvegardés sur la carte SD.
- 🌍 **Toutes les langues de la console** — 15 catalogues, thèmes clair et sombre.
- 🛡️ **Écritures sûres** — le minuteur n'est jamais écrit pendant son décompte, et rien ne tourne en arrière-plan pendant que l'enfant joue.

| Temps de jeu | Limites par jour |
|---|---|
| ![Temps de jeu](images/screenshots/play_timer.png) | ![Limites par jour](images/screenshots/per_day.png) |
| **Activité** | **Un jeu** |
| ![Activité](images/screenshots/activity.png) | ![Un jeu](images/screenshots/activity_game.png) |
| **Restrictions** | **Horloge réseau** |
| ![Restrictions](images/screenshots/restrictions.png) | ![Horloge réseau](images/screenshots/clock.png) |
| **Sécurité et appli** | **Préférences** |
| ![Sécurité et appli](images/screenshots/security.png) | ![Préférences](images/screenshots/preferences.png) |

## Compatibilité

| | Pris en charge | Remarques |
|---|---|---|
| **Firmware** | **21.0.0 → 22.5.0** | La structure de la limite de temps de jeu (0x44 octets) existe depuis 21.0.0 ; en dessous, tous les onglets fonctionnent sauf le temps de jeu. 23.x passe par l'écran [firmware plus récent](#firmware-plus-recent) tant qu'il n'a pas été testé sur console. |
| **Atmosphère** | **1.11.x → 1.12.0** | 1.12.0 ajoute 23.0.0. L'application affiche la version détectée. |
| **Lanceurs** | hbmenu, **sphaira**, **Homebrew App Store** | Le lancement par-dessus un jeu (title override) est recommandé. L'application indique si elle tourne en application ou en applet (album). Par-dessus un jeu, la console compte le temps passé dans PlayGuard comme celui de ce jeu. Dans l'activité, il va au compte choisi au lancement : ouvrez-le avec le compte d'un parent, pas celui d'un enfant. Le minuteur est celui de la console, le même pour tous les comptes : il compte ce temps quel que soit le compte (sauf minuteur désactivé). |
| **Testé sur console** | 22.0.0, 22.1.0, 22.5.0 | 23.0.1 / 1.12.0 : la table des commandes ([switchbrew](https://switchbrew.org/wiki/Parental_Control_services)) a les mêmes commandes, mais rien n'a encore tourné sur console, donc PlayGuard demande d'abord (lecture seule tant que vous n'avez pas choisi) — vos retours sont bienvenus. |
| **Nintendo Switch 2** | Non prise en charge | Atmosphère n'existe pas pour elle. |

<a id="firmware-plus-recent"></a>**Firmware plus récent ?** Au-delà du plus récent firmware testé sur console, PlayGuard s'ouvre **en lecture seule** et vérifie si une version plus récente le prend en charge. Si c'est le cas, il propose de mettre à jour avec sphaira ou le Homebrew App Store. Sinon, vous choisissez : lecture seule, lecture seule avec les outils développeur (pour diagnostiquer le firmware), ou toutes les fonctions à vos risques. Le choix peut être mémorisé pour ce firmware et cette version de l'application ; *Outils › Compatibilité* rouvre l'écran.

**emuMMC et sysMMC.** Chacune a son propre contrôle parental, et PlayGuard ne voit que celui du système sur lequel il tourne. En emuMMC, *Outils* et *Premiers pas* le signalent ; en sysMMC aussi, quand une emuMMC est configurée sur la carte SD. Réglez le contrôle parental sur les deux, ou masquez le menu de démarrage d'hekate pour qu'un enfant ne puisse pas choisir l'autre système.

**Risque de bannissement ?** PlayGuard ne contacte jamais Nintendo : en ligne, il ne joint que des serveurs de temps publics, GitHub (mises à jour) et bpa.st (rapports). Ce qui fait bannir une console, c'est un firmware modifié ou une emuMMC qui se connecte à Nintendo ([FAQ switchbrew](https://switchbrewdocs.readthedocs.io/en/latest/faq.html), en anglais). Aucun bannissement lié à un changement du contrôle parental ou de l'horloge n'est documenté ; ce n'est pas une garantie.

**Notifications sur le téléphone.** Depuis 22.0.0, une console encore associée à l'application mobile peut prévenir le parent quand le code PIN est saisi sur la console ([switchbrew](https://switchbrew.org/wiki/22.0.0), en anglais). On ne sait pas si les déverrouillages de PlayGuard la déclenchent.

## Installation

Au choix :

| Canal | Comment |
|---|---|
| **Homebrew App Store** / **App Store de sphaira** (même catalogue) | Cherchez *PlayGuard* une fois la fiche validée. Les nouvelles versions sont reprises automatiquement. |
| **sphaira › GitHub** | Le zip de la version contient déjà l'entrée (`/config/sphaira/github/playguard.json`) : après une première installation, mettez à jour depuis *GitHub* dans sphaira. |
| **Manuellement** | Téléchargez `playguard.zip` dans la [dernière version](https://github.com/JigSawFr/PlayGuard/releases/latest) et extrayez-le à la **racine** de la carte SD. L'application arrive dans `sd:/switch/playguard/`. |

Le **module système de récupération** optionnel (`playguard-rescue.zip`) se télécharge à part — voir [Console bloquée ?](#console-bloquée--console-doccasion-code-pin-oublié).

<details>
<summary>Fichiers écrits par PlayGuard sur la carte SD</summary>

Tous dans `sd:/switch/playguard/` :

| Chemin | Contenu |
|---|---|
| `config.json` | Préférences (langue, thème, serveur NTP, *Demander le code PIN*…) ; chaque clé dans [docs/config.md](docs/config.md) (en anglais). S'il est illisible, il est gardé en `config.json.bad` (le précédent en `.bad.1`) et les réglages repartent de leurs valeurs par défaut ; PlayGuard le signale une fois au démarrage |
| `history.json` | L'historique des modifications (les 200 dernières) ; s'il est illisible, il est gardé en `history.json.bad` (le précédent en `.bad.1`) plutôt qu'écrasé, et PlayGuard le signale une fois |
| `profiles/` | Profils de limites enregistrés |
| `backups/` | Sauvegardes des réglages (jamais le code PIN) |
| `exports/` | Exports de l'activité |
| `cache/` | La dernière activité de jeu lue (tous les comptes, et chaque compte consulté), affichée dès le lancement suivant pendant que le journal est relu ; les icônes des jeux (`icons/`, 16 Mo au plus, vidé quand la langue de la console change), pour ne pas relire le nom et l'icône de chaque jeu à chaque lancement ; en mode développeur, la liste d'*Installer un autre build* (`dev_builds.json`) |
| `github_token` | Mode développeur uniquement : la connexion GitHub d'*Installer un autre build* (supprimé à la déconnexion) |
| `rescue_report.txt` | Laissé par le sysmodule de secours après son intervention, jusqu'à ce que PlayGuard l'affiche au démarrage (illisible, il est supprimé et PlayGuard le signale) |
| `logs/` | Rapports de diagnostic (jamais le code PIN ni le numéro de série), les fichiers des outils développeur, `uploads.txt` (les liens des rapports envoyés en ligne) et `crash.txt` (ce qui a arrêté PlayGuard, s'il a planté) |

Plus de détails dans [packaging/README.md](packaging/README.md) (en anglais).
</details>

## Premiers pas

1. **Contrôle parental pas encore configuré ?** Ouvrez PlayGuard : le guide *Premiers pas* définit le code PIN (écran système), la limite quotidienne, et vérifie l'horloge réseau.
2. **Associé à l'application mobile ?** *Sécurité et appli › Dissocier l'application mobile* — sinon sa prochaine synchronisation écrase vos réglages.
3. Réglez la limite : *Temps de jeu › Même limite tous les jours* (ou *Limite différente selon le jour…*).
4. **Horloge réseau imprécise** (indiqué dans la Vue d'ensemble) ? Activez *Synchroniser l'horloge via Internet* dans les paramètres de la console, puis *Horloge réseau › Mesurer l'écart* et *Régler l'horloge réseau*.

**Commandes :** ↑/↓ déplacer · Ⓐ valider · Ⓑ retour ou annuler (deux fois sur la barre latérale pour quitter) · Ⓧ actualiser · **+** enregistre les limites par jour.

Ce qui a échoué est dit dans une boîte de dialogue, ce qui a réussi dans une notification. Le titre indique quand l'application est en lecture seule ou tant que le contrôle parental est déverrouillé temporairement ; en lecture seule, les actions restent à leur place, grisées, et disent pourquoi quand on les choisit.

## Les fonctions, onglet par onglet

L'application est organisée en onglets, comme les paramètres de la console. Cliquez sur un onglet pour le déplier.

<details>
<summary><b>Vue d'ensemble</b> — la journée en un coup d'œil</summary>

- **D'abord la journée :** jauge du temps de jeu (quand aucun jeu ne tourne, le temps lu dans le journal d'activité, marqué ≈), limite du jour, temps restant, alarme du coucher. L'alarme « temps écoulé » apparaît en orange tant qu'elle est désactivée — Ⓐ la réactive. Sous la jauge, un rappel : un jeu laissé ouvert sur le menu HOME continue d'être décompté (voir [Ce qui compte comme temps de jeu](#ce-qui-compte-comme-temps-de-jeu)).
- **La console décompte-t-elle ?** Le temps de PlayGuard lui-même compte quand il tourne comme une application : avec une limite aujourd'hui, le temps restant doit donc baisser tant qu'il est ouvert. S'il n'a pas bougé pendant environ 90 s à l'écran (minuteur actif, limite aujourd'hui, pas de déverrouillage, pas d'ouverture depuis l'album), une ligne orange indique *La console ne décompte pas le temps de jeu en ce moment*, avec les causes probables : l'horloge a été reculée (le décompte reprendra plus tard), l'horloge réseau n'a jamais été réglée (onglet *Horloge réseau*), ou une limite vient d'être écrite (lancez un jeu une minute, puis vérifiez). Elle disparaît dès que le temps restant bouge de nouveau. La ligne d'état de Temps de jeu l'indique aussi.
- **Modifié en dehors de PlayGuard :** le temps de jeu du jour reparti de zéro le même jour (un changement d'horloge fait cela), l'horloge de la console déplacée de plus de 5 minutes, des limites différentes de celles que PlayGuard a vues en dernier (l'application mobile, un autre outil). Un avis orange jusqu'à ce qu'il soit masqué ou jusqu'au lendemain, et une entrée dans l'historique des modifications. PlayGuard ne tourne pas en arrière-plan : il ne voit que ce qui a changé pendant qu'il était ouvert, ou depuis sa dernière ouverture, quand il relit la console.
- **À la limite**, la console affiche « temps écoulé ». Sur la console où PlayGuard a été testé (22.0.0), elle a aussi suspendu le jeu, sans « continuer ». Si l'application mobile a déjà été réglée sur « alarme seulement », la console peut se contenter de prévenir : PlayGuard ne peut pas encore lire ni modifier ce réglage ([détails, en anglais](docs/parental-controls.md#when-the-time-runs-out)).
- **Puis l'état :** contrôle parental, code PIN (*Défini* ou *Non défini*, jamais sa longueur), niveau de restriction.
- **Puis ce qui demande attention :** précision de l'horloge réseau, association de l'application mobile (en orange tant qu'elle est associée), firmware / compatibilité seulement en cas de problème, avertissements sur le masquage du numéro de série et les patchs de jeux.
- Les lignes qui ouvrent un autre onglet se terminent par un chevron (›). Ⓐ sur *Limite d'aujourd'hui* la modifie sur place ; Ⓐ sur *Horloge réseau › Imprécise* mesure et règle l'horloge directement.
- **Temps en plus aujourd'hui** (+15 min, +30 min, +1 h par défaut) et **Plus de jeu aujourd'hui** (limite à 0 pour aujourd'hui seulement, *maintenant* ou *dans 5 minutes* quand le temps joué est connu : la limite devient alors le temps joué + 5 min, le temps de sauvegarder ; au reverrouillage, le « temps écoulé » de la console apparaît, et la progression non sauvegardée peut être perdue — la confirmation le signale). Le lendemain — au lancement, ou à minuit si l'application est ouverte — PlayGuard propose de remettre la limite habituelle, ou le fait de lui-même (*Préférences › Remettre d'office la limite habituelle le lendemain*).
- Un bandeau **Verrouiller maintenant** tant que le contrôle parental est déverrouillé temporairement.
- Tant qu'aucun code PIN n'est défini, une ligne **Premiers pas** ouvre le guide en trois étapes, qui s'affiche aussi au lancement.
- Actualisation toutes les 5 s, avec l'heure de la dernière actualisation (Ⓧ pour actualiser tout de suite).
</details>

<details>
<summary><b>Temps de jeu</b> — le graphique de la semaine sert d'éditeur</summary>

- Une ligne d'état : actif, temps restant, limite du jour, profil correspondant (en rouge tant que la limite est atteinte).
- **Le graphique de la semaine :** ←/→ choisit un jour, Ⓐ change sa limite.
- **Même limite tous les jours :** liste rapide ou valeur libre, en minutes (`90`, `90 min`) ou en heures (`1:30`, `1h30`).
- **Limite différente selon le jour :** jours non enregistrés en orange et marqués \*, valeurs rapides, préréglages lundi–vendredi / week-end, « pas de limite » par jour ; **+** enregistre depuis n'importe où.
- **Suppression de la limite**, **temps en plus aujourd'hui**, **plus de jeu aujourd'hui** (comme dans la Vue d'ensemble).
- Une note sous la limite quotidienne dit ce que fait la console à la limite (comme dans la Vue d'ensemble). Pendant un déverrouillage temporaire du contrôle parental, le temps de jeu n'est *peut-être* pas décompté : la question reste ouverte ([docs, en anglais](docs/parental-controls.md#still-open)).
- **Profils** enregistrés sur la carte SD : appliquer, modifier, renommer ou supprimer un profil ; enregistrer les limites actuelles ou en créer un nouveau. N'importe quel nom, dans n'importe quelle écriture (*École*, *周末*, *Выходные* …) — un nom sans lettres latines reçoit son propre nom de fichier, et deux noms qui donneraient le même fichier sont repérés.
- Chaque confirmation dessine la semaine telle qu'elle sera, les jours qui changent en orange.
- **Alarme du coucher** : l'heure de l'alarme (16:00 à 23:45, ou désactivée) et l'heure où le jeu est de nouveau permis (05:00 à 09:00), la même tous les jours. Sa place dans les réglages du minuteur a été déduite des réglages de l'application mobile, pas lue sur une console où une heure du coucher était réglée : PlayGuard ne la modifie que si la console indique ce qu'il y lit, vérifie la réponse de la console après la modification et remet les réglages précédents si elle diffère. Avancé, sur activation : alarme « temps écoulé », pause / reprise du décompte.
- `0` minute signifie *pas de jeu ce jour-là* ; *Supprimer la limite de temps de jeu* désactive le minuteur.
</details>

<details>
<summary><b>Activité</b> — qui a joué à quoi, et combien de temps</summary>

- Temps passé sur chaque jeu **aujourd'hui**, sur les **7 derniers jours** et **depuis toujours**, lu dans le journal d'activité de la console — pour tous les comptes ou **un seul compte utilisateur** (*Compte*, quand la console en a plusieurs).
- Un graphique des sept derniers jours (aujourd'hui à droite, une légende dessous), avec la limite de cette semaine pour chaque jour en trait et le temps au-delà en orange, marqué ! ; totaux du jour, de la semaine et depuis toujours.
- Un **résumé** sur la période choisie, pour le compte affiché : moyenne par jour, le jeu le plus joué, les jours joués et le plus chargé (7 derniers jours), la session moyenne (depuis toujours).
- **À redécouvrir** : les jeux installés, à moins de 3 h au total et pas lancés depuis un mois ou plus, les moins joués d'abord (jusqu'à trois).
- Tri par période ; les premiers jeux affichent leur icône (pas en mode applet, pour économiser la mémoire). Une grande bibliothèque affiche ses 50 premiers jeux, puis *Afficher tous les jeux* ; l'export les contient toujours tous.
- Ⓐ sur un jeu : ses sept derniers jours en barres, ses lancements, ses première et dernière parties, le temps de chaque compte. Les jeux supprimés gardent leurs chiffres depuis toujours, et leur nom dès que PlayGuard l'a vu.
- **Export sur la carte SD** en CSV, JSON, XLSX (Excel) ou PDF, une colonne par jour.
- S'ouvre aussitôt sur les derniers chiffres lus (gardés sur la carte SD d'un lancement à l'autre), actualisés en arrière-plan au-delà d'une minute ; Ⓧ les relit tout de suite, avec un indicateur de chargement.
- Durées approximatives si l'horloge de la console a été modifiée.
</details>

<details>
<summary><b>Restrictions</b> — niveau, classification par âge, communication</summary>

- Niveau de restriction : Aucun, Jeune enfant, Enfant, Adolescent, Personnalisé. Ce qu'impose un préréglage (limite d'âge, publications, communication) est affiché en lecture seule, et dit avant de le choisir.
- En Personnalisé : classification par âge, publications sur les réseaux sociaux, communication avec d'autres joueurs.
- Mode VR, **organisme de classification** (PEGI, ESRB, USK, CERO…).
- Le nombre de jeux autorisés à communiquer (la liste elle-même figure, brute, dans le rapport de diagnostic tant que sa structure n'est pas connue).
</details>

<details>
<summary><b>Horloge réseau</b> — l'horloge dont dépend le minuteur</summary>

- Horloges console et réseau, fuseau horaire, précision.
- Choix d'un serveur NTP public (≈ 50 intégrés, par région, ou le vôtre).
- **Mesure** sur 3 serveurs (médiane ; alerte orange en cas de désaccord), puis **réglage de l'horloge réseau** — une mesure reste utilisable 2 minutes, avec un décompte. La confirmation le dit : changer l'horloge remet à zéro le temps de jeu du jour, toute la limite redevient donc disponible.
- Une console qui n'atteint jamais les serveurs de Nintendo garde cette horloge imprécise, ce qui fausse le minuteur.
</details>

<details>
<summary><b>Sécurité et appli</b> — code PIN, verrous, application mobile</summary>

- **Définir ou changer le code PIN** (écran système), **afficher le code PIN** (après un avertissement et le code PIN lui-même, demandé à chaque fois ; noté dans l'historique, puis un nouveau code PIN est proposé), **déverrouiller temporairement**, **verrouiller maintenant**.
- **Demander le code PIN** dans PlayGuard lui-même : *Jamais*, *Avant une modification* (par défaut : tout le monde peut regarder, seul le parent modifie ; redemandé après 5 min) ou *Pour ouvrir PlayGuard* (un écran verrouillé au lancement, et de nouveau quand PlayGuard revient après 5 min ou plus hors de l'écran). Vérifié dans la couche service, donc aucune modification n'y échappe ; reverrouiller ne le demande jamais. *Afficher le code PIN* le demande à chaque fois, dans tous les modes et même pendant les 5 minutes, et un `config.json` absent ou abîmé compte comme *Avant une modification*. Quitter alors que le contrôle parental est encore déverrouillé propose de le reverrouiller.
- **Verrou de console :** un interrupteur qui met la limite de chaque jour à 0, donc un code PIN est nécessaire pour lancer un jeu — un verrou léger, sans classification par âge ni limite de communication. Il bloque le lancement des jeux, pas le menu HOME, et nécessite un code PIN. Les limites précédentes reviennent quand on le désactive. Tant qu'il est activé, *temps en plus* et *plus de jeu aujourd'hui* sont refusés, et des limites réglées autrement (un profil, une sauvegarde, l'historique…) le remplacent.
- **Application mobile :** association de l'application Contrôle parental Nintendo Switch, dernière synchronisation, et **dissociation** (sinon sa prochaine synchronisation écrase les limites réglées ici). Tant qu'elle est associée, *Avant de dissocier : aidez à décoder…* ouvre la comparaison du bloc (voir [Contribuer](#contribuer)).
- **Supprimer tout le contrôle parental :** deux confirmations, irréversible ; une sauvegarde des réglages est d'abord enregistrée.
</details>

<details>
<summary><b>Préférences</b></summary>

- Langue (toutes celles de la console, ou celle de la console) et thème (clair / sombre, avec proposition de relancer).
- L'onglet d'ouverture.
- **Reverrouiller automatiquement après une modification** (activé par défaut).
- Montants du temps en plus (+15/+30/+1 h, +10/+20/+30 min…) et remise de la limite habituelle le lendemain sans demander.
- Vérification de l'horloge réseau au lancement (une notification si elle a plus d'une minute d'écart ; elle ne règle jamais l'horloge).
- Un **rappel mensuel pour soutenir PlayGuard** (activé par défaut, jamais le premier mois ni juste après une mise à jour ; *Ne plus afficher* sur le rappel ou cet interrupteur le désactive pour de bon, mises à jour comprises).
- Actions avancées.
</details>

<details>
<summary><b>Outils</b> et <b>À propos</b> — historique, sauvegardes, infos console ; version, mises à jour, nouveautés, crédits</summary>

- **Historique des modifications :** ce que PlayGuard a changé (limites, niveau de restriction, code PIN, déverrouillages, dissociation, horloge, restaurations…), quand et depuis où. Ⓐ sur une modification l'affiche et, pour une valeur, **remet la précédente** — avec le même déverrouillage et le même code PIN que toute modification, en signalant si elle a changé depuis.
- **Sauvegarder / restaurer les réglages** sur la carte SD : niveau de restriction, réglages personnalisés, mode VR, organisme de classification, limites quotidiennes, alarme « temps écoulé » (avec les actions avancées activées), et pour mémoire le bloc brut du minuteur — jamais le code PIN. La restauration ne liste que ce qui changerait, et signale quand la sauvegarde avait une alarme du coucher (elle n'est pas réécrite). Nombre de sauvegardes conservées au choix.
- **Premiers pas** rouvre le guide (avec une étape *Dissocier l'application mobile* tant qu'elle est associée, une étape *Réactiver l'alarme « temps écoulé »* tant qu'elle est désactivée, une étape *Protéger PlayGuard* une fois le code PIN défini — le choix *Demander le code PIN* —, et un interrupteur pour qu'il ne s'ouvre plus au lancement). Sous *Fermer*, *Soutenir PlayGuard* affiche les QR codes de soutien.
- **Exporter un rapport de diagnostic**, ou **l'envoyer en ligne** (voir [Signaler un bug](#signaler-un-bug)).
- **Console :** firmware, Atmosphère, compatibilité, stockage (emuMMC ou sysMMC, avec une note quand l'autre système a son propre contrôle parental), **masquage du numéro de série** par Atmosphère (en partie caché jusqu'à Ⓐ ; avertissement en emuMMC s'il n'est pas masqué), **patchs de jeux** (sys-patch ou fichiers sigpatches, avec une recommandation de sys-patch quand seuls des fichiers sont utilisés).
- **À propos** (son propre onglet) : version, mode de lancement et dossier des données ; **mises à jour** (recherche maintenant ou une fois par jour au lancement ; *Mettre à jour avec* sphaira, le Homebrew App Store ou à la main) ; les **nouveautés** de la version installée (son entrée du changelog intégré, en anglais) ; les crédits, comment **soutenir PlayGuard** ([GitHub Sponsors](https://github.com/sponsors/JigSawFr), [Ko-fi](https://ko-fi.com/jigsawfr), affichés en QR codes à scanner avec un téléphone), et un petit *Made in France* 🇫🇷. Après une mise à jour, PlayGuard s'ouvre une fois sur **Nouveautés de la X.Y.Z** (les mêmes notes, puis les QR codes).
</details>

## Sûr par conception

**Écriture sûre de la limite.** Écraser la configuration du minuteur pendant son décompte déstabilise Atmosphère. PlayGuard vérifie donc l'état d'abord ; si le minuteur est actif, la confirmation indique que le contrôle parental sera déverrouillé temporairement — avec le code PIN enregistré, **inutile de vous en souvenir**. Un seul appui déverrouille, vérifie que le système confirme bien le déverrouillage, écrit, puis **reverrouille aussitôt** (ou le propose, si *Reverrouiller automatiquement après une modification* est désactivé). La couche service revérifie l'état juste avant d'écrire : aucun écran ne peut contourner cette protection.

> [!CAUTION]
> Une limite inférieure au temps déjà joué aujourd'hui suspend le jeu dès que le contrôle parental est reverrouillé. La confirmation le signale quand c'est le cas.

**Plus de plantage en 22.5.** `pctl:a`, le service privilégié du contrôle parental, n'accepte **qu'une seule session**. Les anciennes versions de l'application d'origine la gardaient ouverte : l'écran PIN du menu HOME (ou l'applet PIN) ne pouvait pas l'obtenir et Atmosphère pouvait planter. PlayGuard ouvre la session pour chaque action et la libère aussitôt ; les rafraîchissements périodiques s'arrêtent quand l'application est en arrière-plan. *(Diagnostic du [fork d'anbingxi](https://github.com/anbingxi/NX-Pctl-Manager/tree/diag/fw22-5-readonly).)*

**Rien ne tourne en arrière-plan.** La limite, le code PIN, les avertissements et la suspension sont ceux de la console ; PlayGuard ne change que leurs réglages.

## Ce qui compte comme temps de jeu

Le minuteur est celui de la console : PlayGuard le lit, il ne décompte rien lui-même. Ce que l'on sait, et avec quelle certitude ([docs/parental-controls.md](docs/parental-controls.md#time-spent-and-time-left), en anglais, donne les mesures) :

| Situation | Décompté ? | Certitude |
|---|---|---|
| Un jeu en cours | Oui | mesuré (22.0.0) |
| **Un jeu laissé ouvert sur le menu HOME** (suspendu en arrière-plan) | **Oui** : fermez-le, ou mettez la console en veille | rapporté par des parents ([Arqade](https://gaming.stackexchange.com/questions/398954/)) ; cohérent avec les mesures (la console décompte tant qu'une application est ouverte) |
| Mode veille | Non | rapporté ([Arqade](https://gaming.stackexchange.com/questions/398954/)) ; pas mesuré par PlayGuard |
| **PlayGuard ouvert par-dessus un jeu** (en application) | **Oui**, comme ce jeu | mesuré (22.0.0) : ouvrez-le avec un utilisateur parent, ou depuis l'album pour un coup d'œil |
| PlayGuard ouvert depuis l'album (mode applet) | Non | déduit : ce n'est pas une application, et le temps hors des applications ne semblait pas décompté |
| Pendant un déverrouillage temporaire du contrôle parental | Peut-être pas | pas mesuré : question ouverte |
| Après un changement d'horloge | La journée repart de zéro ; après un retour en arrière, le décompte s'arrête jusqu'à ce que l'horloge dépasse l'ancienne heure | observé (22.0.0) ; la Vue d'ensemble indique quand la console ne décompte pas |

## Console bloquée ? (console d'occasion, code PIN oublié)

PlayGuard ne voit que le contrôle parental du système sur lequel il tourne : **l'emuMMC et la sysMMC ont chacune le leur** (un code PIN supprimé sur l'une est toujours là sur l'autre). Lancez-le sur chacune de celles à corriger.

| Situation | Que faire |
|---|---|
| **Console d'occasion :** vous connaissez le code PIN, mais l'application mobile de l'ancien propriétaire est toujours associée (la dissociation échoue, ou la réinitialisation demande son compte) | *Sécurité et appli › Dissocier l'application mobile*, puis, si vous ne voulez plus du tout de contrôle parental, *Supprimer tout le contrôle parental*. Les deux fonctionnent hors ligne, en emuMMC comme en sysMMC. |
| **Code PIN oublié** | *Afficher le code PIN* demande lui-même le code PIN : il ne peut pas aider ici. Avec le module système de récupération installé (ci-dessous), déposez un fichier `RESCUE` et redémarrez : l'écran de récupération en définit alors un nouveau (il n'affiche jamais l'ancien). Sans lui, *Supprimer tout le contrôle parental* permet de repartir de zéro (une sauvegarde des réglages est d'abord enregistrée ; elle ne contient jamais le code PIN) : avec *Demander le code PIN* actif (par défaut, *Avant une modification*), mettez d'abord la carte SD dans un ordinateur et passez `"pin_lock"` à `"off"` dans `sd:/switch/playguard/config.json`. |
| **Le minuteur bloque tout (limite à 0 min) et le code PIN est oublié** | PlayGuard lui-même ne peut pas démarrer. Installez **à l'avance** le module système de récupération optionnel (`playguard-rescue.zip`) ; une fois bloqué, déposez un fichier vide `switch/playguard/RESCUE` sur la carte SD et démarrez — il déverrouille la console pour que PlayGuard puisse s'ouvrir. Voir [`sysmodule/README.md`](sysmodule/README.md) (en anglais). |
| **Console non modifiée** | PlayGuard ne peut rien faire : il nécessite Atmosphère. La procédure officielle passe par la clé maîtresse du service client de Nintendo. |

## PlayGuard ou un sysmodule de remplacement ?

PlayGuard pilote le contrôle parental intégré à la console (le service `pctl`), hors ligne. L'autre approche le remplace par un sysmodule, par exemple [NS Parental Control](https://github.com/TristanIsrael/NSParentalControl) (un sysmodule et un overlay Tesla / Ultrahand).

| | PlayGuard | Un sysmodule de remplacement |
|---|---|---|
| Limite de temps de jeu | Une pour la console (le minuteur de la console) | Une par compte utilisateur |
| Code PIN, avertissements, suspension | Ceux de la console | Les siens |
| En arrière-plan | Rien (le module de récupération optionnel n'agit qu'au démarrage, quand un fichier `RESCUE` le demande) | Un sysmodule, dès le démarrage |
| Après une mise à jour du firmware | Le contrôle de la console continue de fonctionner ; PlayGuard s'ouvre en lecture seule jusqu'à une version qui prend le firmware en charge | Le contrôle dépend du bon fonctionnement du sysmodule |
| Supprimé de la carte SD | Les limites restent sur la console | Le contrôle disparaît avec lui |

**Besoin d'une limite différente pour chaque enfant ?** Choisissez un sysmodule de remplacement — le minuteur de la console n'a qu'une limite pour toute la console. Sinon, PlayGuard garde le contrôle parental de Nintendo et ramène simplement ses réglages sur la console.

## Signaler un bug

1. *Outils › Exporter un rapport de diagnostic* enregistre un fichier texte dans `sd:/switch/playguard/logs/` : firmware, version d'Atmosphère, horloges, stockage, masquage du numéro de série, état des patchs de jeux, et le résultat brut de chaque requête au contrôle parental. **Il ne contient jamais le code PIN ni le numéro de série.**
2. [Ouvrez un ticket](https://github.com/JigSawFr/PlayGuard/issues/new) (en anglais ou en français) et joignez-le.

Ou, console connectée, *Outils › Envoyer un rapport en ligne* (Ⓧ sur le rapport de diagnostic en mode développeur) l'envoie, avec les fichiers de débogage (la référence du bloc du minuteur, la fin de l'enregistrement du minuteur, l'historique des modifications et les réglages de PlayGuard), sur [bpa.st](https://bpa.st) ou, quand PlayGuard est connecté à GitHub (outils développeur), dans un gist secret de votre compte (proposé en premier), après avoir indiqué exactement ce qui part. PlayGuard affiche le lien et deux QR codes : le rapport, et le formulaire de signalement de bug avec le lien et vos versions déjà remplis. Toute personne qui a le lien peut lire le rapport : pendant un mois sur bpa.st, qui le supprime ensuite, ou jusqu'à ce que vous supprimiez le gist sur GitHub. Les liens sont conservés dans `logs/uploads.txt`, avec le lien de suppression de bpa.st pour effacer une paste plus tôt. Un rapport enregistré plus tôt dans `logs/` peut être envoyé de la même façon.

<details>
<summary>Mode développeur (examiner un nouveau firmware)</summary>

Appuyez sept fois sur *À propos › Version* ; les outils développeur apparaissent à la fin d'*Outils*. Il ajoute :

- un interrupteur **lecture seule** — activé, l'application ne peut rien modifier : c'est la façon sûre d'examiner un nouveau firmware (l'écran firmware le propose directement) ;
- le rapport de diagnostic à l'écran (Ⓨ l'enregistre, Ⓧ l'envoie en ligne), et un raccourci pour l'exporter depuis l'onglet Temps de jeu ;
- **Installer un autre build** sur place, pour tester un correctif avant sa publication : la dernière version publiée, l'un des 20 derniers commits de `main`, ou le dernier build d'une pull request ouverte depuis une branche de ce dépôt (jamais depuis un fork : son code n'a pas encore été relu). La version publiée ne demande rien ; les autres sont les artefacts du workflow de build, que GitHub ne donne qu'aux utilisateurs connectés : **Compte GitHub** connecte PlayGuard avec un code et un QR code à scanner avec un téléphone (seules autorisations demandées : lire les fichiers du workflow de build (Actions) et créer des gists, pour *Envoyer un rapport en ligne* ; sinon le jeton ne peut lire que ce qui est public ; il est gardé dans `github_token`, jamais envoyé avec un rapport, et *Compte GitHub* permet de se déconnecter). PlayGuard télécharge le build, le vérifie (taille, empreinte SHA-256 enregistrée par GitHub, en-tête NRO), le met à la place de son propre `.nro` et redémarre dessus (le code PIN est d'abord demandé dès qu'il y en a un). La liste est gardée 10 minutes et s'affiche aussitôt (sa dernière ligne, *Actualiser la liste*, la recharge). La même liste permet de revenir à la version publiée à tout moment ; *À propos › Version* affiche le commit en mode développeur. Les artefacts expirent au bout de 90 jours ;
- **Comparer le bloc du minuteur** (aussi dans *Sécurité et appli* tant que l'application mobile est associée), pour décoder les réglages que PlayGuard n'affiche pas encore : enregistrez le bloc brut comme référence, changez un réglage dans l'application mobile, revenez — PlayGuard liste les valeurs qui ont changé (enregistrées dans `logs/` sur demande). « Alarme seulement » / « suspendre le logiciel » pourrait être trouvé ainsi, et les champs du coucher confirmés. Joignez ce fichier à un ticket, ou envoyez-le avec le rapport (*Envoyer un rapport en ligne*).
- **Enregistrer le temps de jeu** : toutes les 30 s tant que PlayGuard est ouvert, une ligne de ce que rapporte le minuteur (temps restant, temps écoulé, bloc brut des réglages…) dans `logs/play_timer_log.csv`, un fichier qui s'ouvre dans un tableur, plus une ligne juste après chaque modification faite par PlayGuard, qui dit laquelle (la dernière colonne, `event`), et une note quand l'horloge a bougé plus que le temps écoulé. Laissé ouvert après minuit, ou jusqu'à la fin du temps, il montre ce qu'un seul rapport ne peut pas montrer : quand le temps écoulé repart à zéro, ce que dit la console vers la fin. L'interrupteur est mémorisé ; il n'enregistre qu'en mode développeur. Ce qu'on sait déjà est dans [docs/parental-controls.md](docs/parental-controls.md) (en anglais).
</details>

## Contribuer

La compilation, le simulateur de bureau, l'architecture du code, le processus de publication et la traduction sont décrits dans **[CONTRIBUTING.md](CONTRIBUTING.md)** (en anglais). La suite, et les idées en attente d'une décision : **[ROADMAP.md](ROADMAP.md)** (en anglais).

**Sans code, en deux minutes :** si votre console est encore associée à l'application mobile, *Sécurité et appli › Avant de dissocier : aidez à décoder…* enregistre le bloc du minuteur ; vous changez un réglage dans l'application mobile (« alarme seule », ou un coucher pour un seul jour) et PlayGuard liste ce qui a changé. Joignez ce fichier à une issue : ce sont les données que la [feuille de route](ROADMAP.md#waiting-on-data-from-a-console) attend le plus.

En bref :

```sh
make test      # tests unitaires de la couche service C (sans devkitPro)
make desktop   # la vraie interface sous Linux, sur une console simulée
make dist      # playguard.zip (devkitPro switch-dev)
```

Les traductions par des locuteurs natifs sont particulièrement bienvenues — PlayGuard existe en anglais, français (France, Canada), allemand, espagnol (Espagne, Amérique latine), italien, néerlandais, portugais (Portugal, Brésil), russe, japonais, coréen et chinois (simplifié, traditionnel).

## Licence et remerciements

GPLv3 — voir [`LICENSE`](LICENSE). Maintenu par **[JigSawFr](https://github.com/JigSawFr)**.

- Fork de **Pctl Manager** de **Taylor** ([tailiang2008](https://github.com/tailiang2008)) (v2–v3) : couche de service pctl, garde-fou d'écriture du minuteur et interface borealis d'origine. Son historique est dans [CHANGELOG.md](CHANGELOG.md).
- Interface : **[borealis](https://github.com/xfangfang/borealis)** (Apache 2.0), figé dans `extern/borealis/`.
- QR codes : **[QR Code generator](https://github.com/nayuki/QR-Code-generator)** de Project Nayuki (MIT), dans `extern/qrcodegen/`.
- Diagnostic fw 22.5, libération de session et synchronisation NTP adaptés de **[anbingxi/NX-Pctl-Manager](https://github.com/anbingxi/NX-Pctl-Manager/tree/diag/fw22-5-readonly)**.
- Référence des commandes : [switchbrew — Parental Control services](https://switchbrew.org/wiki/Parental_Control_services).

> [!NOTE]
> **Comment PlayGuard est développé.** Des assistants de code à base d'IA ont été utilisés pendant le développement : écriture et relecture du code, traductions, documentation. Les changements de PlayGuard sont pilotés, relus et validés par un développeur professionnel, couverts par les tests unitaires C et le simulateur desktop, et testés sur une vraie console (22.0.0, 22.1.0 et 22.5.0 / Atmosphère 1.11.x) avant publication.

PlayGuard n'est ni affilié à Nintendo ni approuvé par Nintendo. Nintendo Switch est une marque de Nintendo.
