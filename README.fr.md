# PlayGuard

![PlayGuard](images/store/banner.png)

[![build](https://github.com/JigSawFr/PlayGuard/actions/workflows/build.yml/badge.svg)](https://github.com/JigSawFr/PlayGuard/actions/workflows/build.yml)
[![dernière version](https://img.shields.io/github/v/release/JigSawFr/PlayGuard)](https://github.com/JigSawFr/PlayGuard/releases/latest)
[![téléchargements](https://img.shields.io/github/downloads/JigSawFr/PlayGuard/total)](https://github.com/JigSawFr/PlayGuard/releases)
[![licence : GPLv3](https://img.shields.io/badge/licence-GPLv3-blue.svg)](LICENSE)

[English](README.md) · **Français**

**Le contrôle parental de la Nintendo Switch, directement sur la console — sans application mobile, sans compte Nintendo, sans Internet.**

PlayGuard est un homebrew qui ramène sur la console, hors ligne, les réglages de l'application mobile Contrôle parental Nintendo Switch : limite quotidienne de temps de jeu, restrictions, code PIN, horloge réseau, activité de jeu — et un moyen de reprendre la main quand la console est bloquée.

![Vue d'ensemble](images/screenshots/dashboard_fr_dark.png)

> [!WARNING]
> **Nécessite un firmware personnalisé (Atmosphère).** PlayGuard dialogue avec le service système restreint `pctl` : il ne fonctionne que sur une console modifiée. Il ne contourne aucune vérification de compte ou en ligne, et certaines des commandes utilisées sont des commandes `*ForDebug`. **À utiliser à vos risques.**

## Sommaire

- [Points forts](#points-forts)
- [Compatibilité](#compatibilité)
- [Installation](#installation)
- [Premiers pas](#premiers-pas)
- [Les fonctions, onglet par onglet](#les-fonctions-onglet-par-onglet)
- [Sûr par conception](#sûr-par-conception)
- [Console bloquée ?](#console-bloquée--console-doccasion-code-pin-oublié)
- [PlayGuard ou un sysmodule de remplacement ?](#playguard-ou-un-sysmodule-de-remplacement-)
- [Signaler un bug](#signaler-un-bug)
- [Contribuer](#contribuer)
- [Licence et remerciements](#licence-et-remerciements)

## Points forts

- ⏱️ **Limite quotidienne de temps de jeu** — la même tous les jours ou une par jour, modifiée sur un graphique de la semaine ; **profils** enregistrés (*Semaine d'école*, *Vacances d'été*…) ; **temps en plus aujourd'hui** et **plus de jeu aujourd'hui** en un appui.
- 📊 **Activité** — temps par jeu aujourd'hui, sur 7 jours et depuis toujours, par compte utilisateur, avec graphiques et **export** en CSV, JSON, XLSX ou PDF.
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

## Compatibilité

| | Pris en charge | Remarques |
|---|---|---|
| **Firmware** | **21.0.0 → 23.0.1** | La structure de la limite de temps de jeu (0x44 octets) existe depuis 21.0.0 ; en dessous, tous les onglets fonctionnent sauf le temps de jeu. |
| **Atmosphère** | **1.11.x → 1.12.0** | 1.12.0 ajoute 23.0.0. L'application affiche la version détectée. |
| **Lanceurs** | hbmenu, **sphaira**, **Homebrew App Store** | Le lancement par-dessus un jeu (title override) est recommandé. L'application indique si elle tourne en application ou en applet (album). |
| **Testé sur console** | 22.1.0 / Atmosphère 1.11.1 | 23.0.1 / 1.12.0 est couvert par la table des commandes ([switchbrew](https://switchbrew.org/wiki/Parental_Control_services)) mais pas encore testé sur console — vos retours sont bienvenus. |

**Firmware plus récent ?** PlayGuard s'ouvre **en lecture seule** et vérifie si une version plus récente le prend en charge. Si c'est le cas, il propose de mettre à jour avec sphaira ou le Homebrew App Store. Sinon, vous choisissez : lecture seule, lecture seule avec les outils développeur (pour diagnostiquer le firmware), ou toutes les fonctions à vos risques. Le choix peut être mémorisé pour ce firmware et cette version de l'application ; *Outils et à propos › Compatibilité* rouvre l'écran.

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
| `config.json` | Préférences (langue, thème, serveur NTP, *Demander le code PIN*…) |
| `history.json` | L'historique des modifications (les 200 dernières) |
| `profiles/` | Profils de limites enregistrés |
| `backups/` | Sauvegardes des réglages (jamais le code PIN) |
| `exports/` | Exports de l'activité |
| `logs/` | Rapports de diagnostic (jamais le code PIN ni le numéro de série) |

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

- **D'abord la journée :** jauge du temps de jeu (quand aucun jeu ne tourne, le temps lu dans le journal d'activité, marqué ≈), limite du jour, temps restant, alarme du coucher. L'alarme « temps écoulé » apparaît en orange tant qu'elle est désactivée — Ⓐ la réactive.
- **Puis l'état :** contrôle parental, code PIN, niveau de restriction.
- **Puis ce qui demande attention :** précision de l'horloge réseau, association de l'application mobile (en orange tant qu'elle est associée), firmware / compatibilité seulement en cas de problème, avertissements sur le masquage du numéro de série et les patchs de jeux.
- Les lignes qui ouvrent un autre onglet se terminent par un chevron (›). Ⓐ sur *Limite d'aujourd'hui* la modifie sur place ; Ⓐ sur *Horloge réseau › Imprécise* mesure et règle l'horloge directement.
- **Temps en plus aujourd'hui** (+15 min, +30 min, +1 h par défaut) et **Plus de jeu aujourd'hui** (limite à 0 pour aujourd'hui seulement ; le jeu en cours est suspendu au reverrouillage, ce que la confirmation signale). Le lendemain — au lancement, ou à minuit si l'application est ouverte — PlayGuard propose de remettre la limite habituelle, ou le fait de lui-même (*Préférences › Remettre d'office la limite habituelle le lendemain*).
- Un bandeau **Verrouiller maintenant** tant que le contrôle parental est déverrouillé temporairement.
- Tant qu'aucun code PIN n'est défini, une ligne **Premiers pas** ouvre le guide en trois étapes, qui s'affiche aussi au lancement.
- Actualisation toutes les 5 s, avec l'heure de la dernière actualisation (Ⓧ pour actualiser tout de suite).
</details>

<details>
<summary><b>Temps de jeu</b> — le graphique de la semaine sert d'éditeur</summary>

- Une ligne d'état : actif, temps restant, limite du jour, profil correspondant (en rouge tant que la limite est atteinte).
- **Le graphique de la semaine :** ←/→ choisit un jour, Ⓐ change sa limite.
- **Même limite tous les jours :** liste rapide ou valeur libre, en minutes (`90`, `90 min`) ou en heures (`1:30`, `1h30`).
- **Limite différente selon le jour :** jours non enregistrés en orange, valeurs rapides, préréglages lundi–vendredi / week-end, « pas de limite » par jour ; **+** enregistre depuis n'importe où.
- **Suppression de la limite**, **temps en plus aujourd'hui**, **plus de jeu aujourd'hui** (comme dans la Vue d'ensemble).
- **Profils** enregistrés sur la carte SD : appliquer, modifier, renommer ou supprimer un profil ; enregistrer les limites actuelles ou en créer un nouveau. N'importe quel nom, accents compris — deux noms qui donneraient le même fichier sont repérés.
- Chaque confirmation dessine la semaine telle qu'elle sera, les jours qui changent en orange.
- Alarme du coucher (lecture seule). Avancé, sur activation : alarme « temps écoulé », pause / reprise du décompte.
- `0` minute signifie *pas de jeu ce jour-là* ; *Supprimer la limite de temps de jeu* désactive le minuteur.
</details>

<details>
<summary><b>Activité</b> — qui a joué à quoi, et combien de temps</summary>

- Temps passé sur chaque jeu **aujourd'hui**, sur les **7 derniers jours** et **depuis toujours**, lu dans le journal d'activité de la console — pour tous les comptes ou **un seul compte utilisateur** (*Compte*, quand la console en a plusieurs).
- Un graphique des sept derniers jours, avec la limite de chaque jour en trait et le temps au-delà en orange ; totaux du jour, de la semaine et depuis toujours.
- Tri par période ; les premiers jeux affichent leur icône (pas en mode applet, pour économiser la mémoire).
- Ⓐ sur un jeu : ses sept derniers jours en barres, ses lancements, ses première et dernière parties, le temps de chaque compte. Les jeux supprimés gardent leurs chiffres depuis toujours.
- **Export sur la carte SD** en CSV, JSON, XLSX (Excel) ou PDF, une colonne par jour.
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
- **Mesure** sur 3 serveurs (médiane ; alerte orange en cas de désaccord), puis **réglage de l'horloge réseau** — une mesure reste utilisable 2 minutes, avec un décompte.
- Une console qui n'atteint jamais les serveurs de Nintendo garde cette horloge imprécise, ce qui fausse le minuteur.
</details>

<details>
<summary><b>Sécurité et appli</b> — code PIN, verrous, application mobile</summary>

- **Définir ou changer le code PIN** (écran système), **afficher le code PIN** (après un avertissement, en cas d'oubli), **déverrouiller temporairement**, **verrouiller maintenant**.
- **Demander le code PIN** dans PlayGuard lui-même : *Jamais*, *Avant une modification* (tout le monde peut regarder, seul le parent modifie ; redemandé après 5 min) ou *Pour ouvrir PlayGuard*. Vérifié dans la couche service, donc aucune modification n'y échappe ; reverrouiller ne le demande jamais.
- **Verrou de console :** un interrupteur qui met la limite de chaque jour à 0, donc un code PIN est nécessaire pour lancer un jeu — un verrou léger, sans classification par âge ni limite de communication. Il bloque le lancement des jeux, pas le menu HOME, et nécessite un code PIN. Les limites précédentes reviennent quand on le désactive.
- **Application mobile :** association de l'application Contrôle parental Nintendo Switch, dernière synchronisation, et **dissociation** (sinon sa prochaine synchronisation écrase les limites réglées ici).
- **Supprimer tout le contrôle parental :** deux confirmations, irréversible ; une sauvegarde des réglages est d'abord enregistrée.
</details>

<details>
<summary><b>Préférences</b></summary>

- Langue (toutes celles de la console, ou celle de la console) et thème (clair / sombre, avec proposition de relancer).
- L'onglet d'ouverture.
- **Reverrouiller automatiquement après une modification** (activé par défaut).
- Montants du temps en plus (+15/+30/+1 h, +10/+20/+30 min…) et remise de la limite habituelle le lendemain sans demander.
- Vérification de l'horloge réseau au lancement (une notification si elle a plus d'une minute d'écart ; elle ne règle jamais l'horloge).
- Actions avancées.
</details>

<details>
<summary><b>Outils et à propos</b> — historique, sauvegardes, mises à jour, infos console</summary>

- **Historique des modifications :** ce que PlayGuard a changé (limites, niveau de restriction, code PIN, déverrouillages, dissociation, horloge, restaurations…), quand et depuis où. Ⓐ sur une modification l'affiche et, pour une valeur, **remet la précédente** — avec le même déverrouillage et le même code PIN que toute modification, en signalant si elle a changé depuis.
- **Sauvegarder / restaurer les réglages** sur la carte SD : niveau de restriction, réglages personnalisés, mode VR, organisme de classification, limites quotidiennes, alarme « temps écoulé » (avec les actions avancées activées), et pour mémoire le bloc brut du minuteur — jamais le code PIN. La restauration ne liste que ce qui changerait. Nombre de sauvegardes conservées au choix.
- **Premiers pas** rouvre le guide (avec une étape *Dissocier l'application mobile* tant qu'elle est associée, une étape *Réactiver l'alarme « temps écoulé »* tant qu'elle est désactivée, et un interrupteur pour qu'il ne s'ouvre plus au lancement).
- **Mises à jour :** recherche maintenant ou une fois par jour au lancement ; *Mettre à jour avec* sphaira, le Homebrew App Store ou à la main.
- **Exporter un rapport de diagnostic** (voir [Signaler un bug](#signaler-un-bug)).
- **Console :** firmware, Atmosphère, compatibilité, stockage (emuMMC ou sysMMC), **masquage du numéro de série** par Atmosphère (en partie caché jusqu'à Ⓐ ; avertissement en emuMMC s'il n'est pas masqué), **patchs de jeux** (sys-patch ou fichiers sigpatches, avec une recommandation de sys-patch quand seuls des fichiers sont utilisés).
</details>

## Sûr par conception

**Écriture sûre de la limite.** Écraser la configuration du minuteur pendant son décompte déstabilise Atmosphère. PlayGuard vérifie donc l'état d'abord ; si le minuteur est actif, la confirmation indique que le contrôle parental sera déverrouillé temporairement — avec le code PIN enregistré, **inutile de vous en souvenir**. Un seul appui déverrouille, vérifie que le système confirme bien le déverrouillage, écrit, puis **reverrouille aussitôt** (ou le propose, si *Reverrouiller automatiquement après une modification* est désactivé). La couche service revérifie l'état juste avant d'écrire : aucun écran ne peut contourner cette protection.

> [!CAUTION]
> Une limite inférieure au temps déjà joué aujourd'hui suspend le jeu dès que le contrôle parental est reverrouillé. La confirmation le signale quand c'est le cas.

**Plus de plantage en 22.5.** `pctl:a`, le service privilégié du contrôle parental, n'accepte **qu'une seule session**. Les anciennes versions de l'application d'origine la gardaient ouverte : l'écran PIN du menu HOME (ou l'applet PIN) ne pouvait pas l'obtenir et Atmosphère pouvait planter. PlayGuard ouvre la session pour chaque action et la libère aussitôt ; les rafraîchissements périodiques s'arrêtent quand l'application est en arrière-plan. *(Diagnostic du [fork d'anbingxi](https://github.com/anbingxi/NX-Pctl-Manager/tree/diag/fw22-5-readonly).)*

**Rien ne tourne en arrière-plan.** La limite, le code PIN, les avertissements et la suspension sont ceux de la console ; PlayGuard ne change que leurs réglages.

## Console bloquée ? (console d'occasion, code PIN oublié)

PlayGuard ne voit que le contrôle parental du système sur lequel il tourne : **l'emuMMC et la sysMMC ont chacune le leur** (un code PIN supprimé sur l'une est toujours là sur l'autre). Lancez-le sur chacune de celles à corriger.

| Situation | Que faire |
|---|---|
| **Console d'occasion :** vous connaissez le code PIN, mais l'application mobile de l'ancien propriétaire est toujours associée (la dissociation échoue, ou la réinitialisation demande son compte) | *Sécurité et appli › Dissocier l'application mobile*, puis, si vous ne voulez plus du tout de contrôle parental, *Supprimer tout le contrôle parental*. Les deux fonctionnent hors ligne, en emuMMC comme en sysMMC. |
| **Code PIN oublié** | *Sécurité et appli › Afficher le code PIN*. Ou *Supprimer tout le contrôle parental* pour repartir de zéro (une sauvegarde des réglages est d'abord enregistrée ; elle ne contient jamais le code PIN). |
| **Code PIN oublié, et *Demander le code PIN* réglé sur *Pour ouvrir PlayGuard* ou *Avant une modification*** | Ce réglage est exprès dans `sd:/switch/playguard/config.json` : mettez la carte SD dans un ordinateur et passez `"pin_lock"` à `"off"`. |
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

1. *Outils et à propos › Exporter un rapport de diagnostic* enregistre un fichier texte dans `sd:/switch/playguard/logs/` : firmware, version d'Atmosphère, horloges, stockage, masquage du numéro de série, état des patchs de jeux, et le résultat brut de chaque requête au contrôle parental. **Il ne contient jamais le code PIN ni le numéro de série.**
2. [Ouvrez un ticket](https://github.com/JigSawFr/PlayGuard/issues/new) (en anglais ou en français) et joignez-le.

<details>
<summary>Mode développeur (examiner un nouveau firmware)</summary>

Appuyez sept fois sur *Outils et à propos › Version*. Il ajoute :

- un interrupteur **lecture seule** — activé, l'application ne peut rien modifier : c'est la façon sûre d'examiner un nouveau firmware (l'écran firmware le propose directement) ;
- le rapport de diagnostic à l'écran, et un raccourci pour l'exporter depuis l'onglet Temps de jeu ;
- **Comparer le bloc du minuteur**, pour décoder les réglages que PlayGuard n'affiche pas encore : enregistrez le bloc brut comme référence, changez un réglage dans l'application mobile, revenez — PlayGuard liste les valeurs qui ont changé (enregistrées dans `logs/` sur demande). L'heure du coucher et « alarme seulement » / « suspendre le logiciel » ont pu être trouvés ainsi. Joignez ce fichier à un ticket.
</details>

## Contribuer

La compilation, le simulateur de bureau, l'architecture du code, le processus de publication et la traduction sont décrits dans **[CONTRIBUTING.md](CONTRIBUTING.md)** (en anglais).

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
- Diagnostic fw 22.5, libération de session et synchronisation NTP adaptés de **[anbingxi/NX-Pctl-Manager](https://github.com/anbingxi/NX-Pctl-Manager/tree/diag/fw22-5-readonly)**.
- Référence des commandes : [switchbrew — Parental Control services](https://switchbrew.org/wiki/Parental_Control_services).

PlayGuard n'est ni affilié à Nintendo ni approuvé par Nintendo. Nintendo Switch est une marque de Nintendo.
