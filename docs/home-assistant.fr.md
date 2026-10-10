# PlayGuard et Home Assistant

[English](home-assistant.md) · **Français**

PlayGuard peut publier l'état du contrôle parental de la console et son temps
de jeu sur un broker MQTT, et recevoir des ordres par lui. Home Assistant
affiche alors la console comme un appareil (limites, temps restant, temps de
jeu par jeu, heure du coucher, restrictions) et peut changer les limites,
ajouter du temps ou verrouiller la console depuis un tableau de bord ou une
automatisation. Le lien est **facultatif et désactivé par défaut**, reste sur
votre réseau local et fonctionne avec n'importe quel broker MQTT ; le module
Mosquitto de Home Assistant est le plus simple.

> [!NOTE]
> Seul, le lien fonctionne **tant que PlayGuard est ouvert** sur la console :
> les ordres envoyés quand il est fermé ou que la console est en veille
> attendent sur le broker et sont exécutés à la prochaine ouverture de
> PlayGuard. L'**agent** facultatif le garde en permanence
> ([plus bas](#garder-le-lien-quand-playguard-est-fermé)).

## Ce qu'il faut

- Home Assistant avec l'intégration **MQTT** configurée, et un broker : le
  module **Mosquitto broker** (Paramètres › Modules complémentaires) ou tout
  autre broker MQTT 3.1.1.
- Un utilisateur du broker pour PlayGuard. Avec le module Mosquitto, tout
  utilisateur de Home Assistant peut se connecter : créez-en un nommé
  `playguard` (Paramètres › Personnes › Utilisateurs, « connexion depuis le
  réseau local uniquement »), ou ajoutez un identifiant dans la configuration
  du module.
- La console sur le même réseau que le broker, avec une version de PlayGuard
  qui a *Préférences › Accès à distance*.

## Le configurer sur la console

1. Ouvrez PlayGuard › **Préférences › Accès à distance › MQTT / Home
   Assistant**.
2. **Adresse du broker** : l'adresse IP ou le nom de la machine du broker
   (celle de Home Assistant, pour le module). **Port** : 1883 (8883 pour TLS,
   qui active *Connexion chiffrée*).
3. **Nom d'utilisateur** et **Mot de passe** : l'utilisateur du broker créé
   plus haut. Un broker qui accepte les clients anonymes : *Nom d'utilisateur
   › Aucun*.
4. **Nom dans Home Assistant** : le nom de l'appareil (*Salon*, *Switch de
   Léa*…).
5. Activez **Lien à distance**. La console reçoit un identifiant aléatoire
   (8 caractères, jamais le numéro de série), et **État** affiche *En ligne*
   en quelques secondes.

Changer l'un de ces réglages demande le code PIN quand *Sécurité › Demander le
code PIN* vaut *Avant un changement* : le lien peut modifier la console.

## Garder le lien quand PlayGuard est fermé

L'**agent** est un petit module facultatif en arrière-plan (un sysmodule
Atmosphère, [`sysmodule/agent`](../sysmodule/agent/README.md)) qui tient le
même lien quand PlayGuard est fermé, avec les mêmes réglages :

1. PlayGuard › **Outils › Modules facultatifs › Agent du lien à distance ›
   Installer** (PlayGuard le contient ; ou extrayez `playguard-agent.zip` de
   la release à la racine de la carte SD). Il démarre aussitôt, puis à chaque
   démarrage.
2. Rien d'autre à régler : il lit *Accès à distance*. Son **État** affiche
   alors *En ligne · par l'agent*.

Quand PlayGuard est ouvert, l'agent reste le seul client MQTT de la console
(Home Assistant ne la voit jamais hors ligne) ; PlayGuard lui passe ce qu'il
lit et exécute les ordres, avec sa confirmation, sa vérification du code PIN
et son historique habituels. Quand PlayGuard est fermé, l'agent lit la console
lui-même toutes les `poll_s` secondes et n'exécute les ordres **qu'avec
*Les exécuter aussitôt*** : avec *Demander sur la console*, ils attendent
PlayGuard. Il ne change rien quand PlayGuard était en mode lecture seule ou
après une mise à jour du système que PlayGuard n'a pas encore vérifiée. Ce
qu'il a changé apparaît dans l'historique des changements de PlayGuard, avec
son heure, à la prochaine ouverture de PlayGuard.

## Ce qui apparaît dans Home Assistant

Paramètres › Appareils et services › MQTT : un appareil au nom de la console,
créé par la découverte MQTT (désactivez *Créer l'appareil dans Home
Assistant* pour gérer les topics vous-même).

| Type | Entités |
|---|---|
| Capteurs | Temps d'écran utilisé (le décompte du minuteur), Temps d'écran utilisé (journal de jeu), Temps d'écran restant, Temps d'écran prolongé, En cours de jeu (vide tant que PlayGuard est ouvert : personne ne joue alors), Âge de classification, Firmware, Atmosphère, Dernier ordre |
| Capteurs binaires | Contrôle parental, Déverrouillé temporairement, Code PIN défini, Appli mobile liée, Minuteur, Temps écoulé, Horloge réseau exacte |
| Nombres | Limite dimanche … samedi, Limite tous les jours, Temps d'écran max aujourd'hui (0 à 1440 minutes ; 1440 = pas de limite) |
| Heures | Alarme du coucher, Fin du coucher |
| Interrupteurs | Verrou de console, Alarme « temps écoulé », Déverrouillé, Alarme du coucher activée, Mode VR restreint, Publication sur les réseaux sociaux restreinte, Communication restreinte |
| Sélecteurs | Niveau de restriction, Profil (vos profils enregistrés) |
| Boutons | Synchroniser maintenant, Verrouiller maintenant, Temps en plus +15 / +30 min / +1 h, Fin du jeu pour aujourd'hui, Supprimer la limite, Exporter un rapport de diagnostic |
| Événement | Événements : chaque ordre exécuté ou refusé, avec la raison |

Les entités portent pour l'instant leurs noms anglais (*Used screen time*,
*Limit Monday*…), ceux de l'intégration *Nintendo Switch Parental Controls*
de Home Assistant quand le sens est le même ; renommez-les dans Home Assistant
si besoin. Celles qui changent le minuteur n'apparaissent qu'une fois *Les
ordres peuvent changer le temps de jeu* activé (plus bas).

Les chiffres du journal de jeu (temps par jeu aujourd'hui, les 7 derniers
jours, les jours terminés) sont aussi sur le broker
(`playguard/<id>/activity`, `…/week`, `…/activity/<date>`), pour vos propres
modèles ou une future intégration.

## Les ordres

**Ordres reçus** règle ce que fait PlayGuard d'un ordre :

- **Demander sur la console** (par défaut) : la console affiche l'ordre et
  demande avant de l'exécuter. Personne devant la console, rien ne se passe ;
  l'ordre attend.
- **Les exécuter aussitôt** : aucune question, et pas de code PIN demandé pour
  l'ordre (le broker a déjà vérifié qui l'envoie).
- **Les refuser** : Home Assistant ne fait que regarder.

**Les ordres peuvent changer le temps de jeu** est désactivé par défaut. Sans
lui, le lien peut observer, reverrouiller la console et changer les
restrictions, mais pas les limites, l'heure du coucher ni le temps en plus.
Avec lui, un ordre se passe exactement comme un changement fait sur la
console : quand le minuteur décompte, PlayGuard déverrouille un instant le
contrôle parental avec le code PIN enregistré, écrit, puis reverrouille.

Chaque ordre figure dans l'historique (*Outils › Historique des changements*,
« Lien à distance »). Certaines choses ne sont **jamais** possibles par le
lien : supprimer le contrôle parental, délier l'appli mobile, et lire, définir
ou changer le code PIN. Le mode lecture seule (un firmware que PlayGuard ne
connaît pas encore) refuse tous les ordres.

## Exemples

Un bouton de tableau de bord qui donne 30 minutes de plus :

```yaml
type: button
name: +30 min
icon: mdi:timer-plus-outline
tap_action:
  action: call-service
  service: button.press
  target:
    entity_id: button.salon_extra_time_30_min
```

Les limites du week-end le vendredi soir :

```yaml
alias: Limites du week-end
triggers:
  - trigger: time
    at: "18:00:00"
conditions:
  - condition: time
    weekday: [fri]
actions:
  - action: number.set_value
    target:
      entity_id: [number.salon_limit_saturday, number.salon_limit_sunday]
    data:
      value: 180
```

Une notification quand un ordre est refusé :

```yaml
alias: Ordre PlayGuard refusé
triggers:
  - trigger: state
    entity_id: event.salon_events
conditions:
  - condition: template
    value_template: "{{ trigger.to_state.attributes.event_type == 'command_rejected' }}"
actions:
  - action: notify.notify
    data:
      message: "PlayGuard a refusé {{ trigger.to_state.attributes.entity }} : {{ trigger.to_state.attributes.reason }}"
```

Les identifiants d'entités dépendent du nom de la console ; vérifiez-les sur
la page de l'appareil.

## Sécurité

- Donnez à PlayGuard **un utilisateur du broker qui lui est propre**. Avec un
  Mosquitto classique, une liste d'accès peut le limiter à ses topics :

  ```
  user playguard
  topic readwrite playguard/#
  topic readwrite homeassistant/device/#
  topic read homeassistant/status
  ```

- Quiconque peut publier sur `playguard/<id>/…/set` peut envoyer des ordres.
  Gardez le broker sur votre réseau local, et laissez *Les ordres peuvent
  changer le temps de jeu* désactivé si vous n'en avez pas besoin.
- Les réglages, mot de passe compris, sont dans
  `sd:/switch/playguard/sync.conf`, en clair. Ce fichier n'est jamais dans un
  rapport de diagnostic ni dans un envoi en ligne ; la section *Remote link* du
  rapport ne contient que les interrupteurs et les compteurs.
- **TLS** : port 8883 et *Connexion chiffrée*. La console vérifie le
  certificat du broker avec son propre magasin de certificats ; pour une
  autorité de certification privée, copiez son fichier PEM sur la carte SD et
  ajoutez `ca_file=/switch/playguard/sync/ca.pem` dans `sync.conf`. TLS n'a
  pas encore été essayé sur une console.

## Quand la connexion échoue

*Accès à distance › État* dit pourquoi, et *Journal* montre les dernières
lignes (en anglais) :

| État | À vérifier |
|---|---|
| *Incomplet : l'adresse du broker manque* | Renseignez *Adresse du broker*. |
| *Non connecté : cannot find …* | Le nom n'est pas résolu : utilisez l'adresse IP. |
| *Non connecté : connect: Connection refused* / *no answer from the broker* | Mauvaise adresse ou mauvais port, broker arrêté, ou pare-feu. |
| *Non connecté : the broker refused the connection: bad user name or password* | Le nom d'utilisateur ou le mot de passe. |
| *Non connecté : the broker refused a subscription* | La liste d'accès de l'utilisateur (plus haut). |

La console réessaie d'elle-même : après 5 secondes, puis deux fois plus
longtemps à chaque fois, jusqu'à 5 minutes. *Synchroniser maintenant* republie
tout aussitôt.

## Sous le capot

Les topics, les documents, les ordres et leurs bornes, et chaque clé de
`sync.conf` sont dans [`sync-protocol.md`](sync-protocol.md) ; la conception
et ses raisons dans [`sync-design.md`](sync-design.md).
