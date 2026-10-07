# PlayGuard

*[Read in English](README.md)*

Un gestionnaire du contrôle parental de la Nintendo Switch — **sans application mobile, sans compte Nintendo, sans Internet**. Réglez la limite quotidienne de temps de jeu directement sur la console, modifiez les restrictions, réglez l'horloge réseau, et réinitialisez / supprimez le code PIN ou dissociez l'application mobile.

![Vue d'ensemble](images/screenshots/dashboard_fr_dark.png)

> ⚠️ **Nécessite un firmware personnalisé (Atmosphère).** L'application dialogue avec le service système restreint `pctl` : elle ne fonctionne que sur une console modifiée. Elle ne contourne aucune vérification de compte ou en ligne : elle ramène sur la console, hors ligne, des réglages sinon réservés à l'application mobile (ou cachés dans les paramètres). Certaines commandes utilisées sont des commandes `*ForDebug`. **À utiliser à vos risques.**

## Compatibilité

| | Pris en charge | Remarques |
|---|---|---|
| Firmware | **21.0.0 → 23.0.1** | La structure de la limite de temps de jeu (0x44 octets) existe depuis 21.0.0. En dessous, tous les onglets fonctionnent sauf le temps de jeu. Un firmware plus récent affiche un avertissement unique : la lecture est sûre, vérifiez le résultat des modifications. |
| Atmosphère | **1.11.x → 1.12.0** | 1.12.0 ajoute 23.0.0. L'application affiche la version détectée. |
| Lanceurs | hbmenu, **sphaira**, **Homebrew App Store** | Le lancement par-dessus un jeu (title override) est recommandé ; l'application indique si elle tourne en application ou en applet (album). |
| Testé sur console | 22.1.0 / Atmosphère 1.11.1 | 23.0.1 / 1.12.0 est couvert par la table des commandes (switchbrew) mais pas encore testé sur console : vos retours sont bienvenus. |

**Pourquoi plus de plantage en 22.5.** `pctl:a`, le service privilégié du contrôle parental, n'accepte **qu'une seule session**. Les anciennes versions de l'application d'origine la gardaient ouverte en permanence : l'écran PIN du menu HOME (ou l'applet PIN) ne pouvait pas l'obtenir et Atmosphère pouvait planter. Dans PlayGuard, chaque action ouvre la session, fait son travail et la libère aussitôt ; les rafraîchissements périodiques s'arrêtent quand l'application est en arrière-plan. *(Diagnostic du [fork d'anbingxi](https://github.com/anbingxi/NX-Pctl-Manager/tree/diag/fw22-5-readonly).)*

## Fonctions

L'application est organisée en onglets, comme les paramètres de la console.

| Onglet | Ce que vous pouvez faire |
|---|---|
| **Vue d'ensemble** | État du contrôle parental, code PIN, niveau de restriction, jauge du temps de jeu du jour, alarme du coucher, précision de l'horloge réseau, association de l'application mobile, firmware / Atmosphère / compatibilité. Actualisation toutes les 5 s (X pour actualiser tout de suite). |
| **Temps de jeu** | Même limite tous les jours (liste rapide ou valeur libre), limite différente par jour (préréglages lundi–vendredi / week-end, « pas de limite » par jour), suppression de la limite, **profils** enregistrés sur la carte SD (ex. *Semaine d'école*, *Vacances*), alarme du coucher (lecture seule). Avancé, sur activation : alarme « temps écoulé », pause / reprise du décompte. |
| **Restrictions** | Niveau de restriction (Aucun, Jeune enfant, Enfant, Adolescent, Personnalisé) ; en Personnalisé : classification par âge, publications sur les réseaux sociaux, communication avec d'autres joueurs ; mode VR ; organisme de classification. |
| **Horloge réseau** | Horloges console / réseau, fuseau horaire, précision. Choix d'un serveur NTP public (≈ 50 intégrés, par région, ou le vôtre), **mesure** sur 3 serveurs (médiane, alerte en cas de désaccord) et **réglage de l'horloge réseau**. Le minuteur s'appuie sur cette horloge ; une console qui n'atteint jamais les serveurs de Nintendo la garde imprécise. |
| **Application mobile** | Association de l'application Contrôle parental Nintendo Switch, dernière synchronisation, dissociation (sinon sa prochaine synchronisation écrase les limites réglées ici). |
| **Code PIN et sécurité** | Définir / changer le code PIN (écran système), déverrouiller temporairement, **verrouiller de nouveau maintenant**, supprimer tout le contrôle parental (double confirmation, irréversible). |
| **Outils et à propos** | Export d'un rapport de diagnostic, langue (console / English / Français), thème (console / clair / sombre), actions avancées, versions. |

### Écriture sûre de la limite

Si le minuteur est en cours, écraser sa configuration déstabilise Atmosphère. Avant toute écriture, l'application vérifie donc l'état ; si le minuteur est actif, elle demande confirmation, déverrouille temporairement le contrôle parental (avec le code PIN enregistré — **inutile de vous en souvenir**), vérifie que le système confirme bien le déverrouillage, écrit, puis propose de **reverrouiller aussitôt**. La couche service revérifie le même état juste avant d'écrire : aucun écran ne peut contourner cette protection.

`0` minute signifie *pas de jeu ce jour-là* ; *Supprimer la limite de temps de jeu* désactive le minuteur. ⚠️ Ne fixez pas une limite inférieure au temps déjà joué aujourd'hui : dès que le contrôle parental est reverrouillé, le jeu est suspendu.

## Installation

Au choix :

- **Homebrew App Store** ou **App Store de sphaira** (même catalogue) : cherchez *PlayGuard* une fois la fiche validée. Les nouvelles versions GitHub sont reprises automatiquement.
- **Menu GitHub de sphaira** : le zip de la version contient déjà l'entrée (`/config/sphaira/github/playguard.json`) ; après une première installation, la mise à jour se fait depuis *GitHub* dans sphaira.
- **Manuellement** : téléchargez `playguard.zip` dans les [Releases](../../releases/latest) et extrayez-le à la **racine** de la carte SD. L'application arrive dans `sd:/switch/playguard/`.

### Premiers pas

1. **Contrôle parental pas encore configuré :** Paramètres de la console › Contrôle parental › définissez un code PIN (sans associer l'application mobile). Ouvrez ensuite l'application.
2. **Déjà associée à l'application mobile :** *Application mobile* › *Dissocier*, sinon la prochaine synchronisation écrase vos réglages.
3. *Temps de jeu* › *Même limite tous les jours* (ou *Limite différente selon le jour…*).
4. Si l'horloge réseau n'est pas précise (Vue d'ensemble) : *Horloge réseau* › *Mesurer l'écart* puis *Régler l'horloge réseau* (activez d'abord *Synchroniser l'horloge via Internet* dans les paramètres).

Commandes : ↑/↓ déplacer, Ⓐ valider, Ⓑ retour (sur la barre latérale : quitter), Ⓧ actualiser.

## Signaler un bug

*Outils et à propos* › *Exporter un rapport de diagnostic* enregistre un fichier texte dans `sd:/switch/playguard/logs/` (firmware, version d'Atmosphère, horloges, résultat brut de chaque requête). **Il ne contient jamais le code PIN.** Joignez-le au ticket.

La compilation et l'architecture sont décrites dans le [README anglais](README.md#build-from-source).

## Licence

GPLv3 (voir [`LICENSE`](LICENSE)). PlayGuard est maintenu par **[JigSawFr](https://github.com/JigSawFr)**. C'est un fork de **Pctl Manager** de **Taylor** ([tailiang2008](https://github.com/tailiang2008)) : couche de service pctl, garde-fou d'écriture du minuteur et interface borealis d'origine. Interface : [borealis](https://github.com/xfangfang/borealis) (Apache 2.0). Diagnostic fw 22.5, libération de session et synchronisation NTP adaptés du [fork d'anbingxi](https://github.com/anbingxi/NX-Pctl-Manager/tree/diag/fw22-5-readonly).
