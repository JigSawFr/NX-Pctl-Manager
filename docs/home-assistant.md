# PlayGuard and Home Assistant

**English** · [Français](home-assistant.fr.md)

PlayGuard can publish the console's parental-control state and play time to an
MQTT broker, and take orders from it. Home Assistant then shows the console as
a device (limits, time left, play time per game, bedtime, restrictions) and
can change the limits, add extra time or lock the console from a dashboard or
an automation. The link is **optional and off by default**, stays on your
local network, and works with any MQTT broker; the Mosquitto add-on of Home
Assistant is the easy path.

> [!NOTE]
> On its own the link runs **while PlayGuard is open** on the console: orders
> sent while it is closed or the console sleeps wait on the broker and are
> carried out the next time PlayGuard opens. The optional **agent** keeps it up
> all the time ([below](#keep-the-link-up-when-playguard-is-closed)).

## What you need

- Home Assistant with the **MQTT** integration set up, and a broker: the
  **Mosquitto broker** add-on (Settings › Add-ons) or any other MQTT 3.1.1
  broker.
- A broker user for PlayGuard. With the Mosquitto add-on, any Home Assistant
  user can log in: create one named `playguard` (Settings › People › Users,
  "can only log in from the local network"), or add a login in the add-on's
  configuration.
- The console on the same network as the broker, with a PlayGuard that has
  *Preferences › Remote access*.

## Set it up on the console

1. Open PlayGuard › **Preferences › Remote access › MQTT / Home Assistant**.
2. **Broker address**: the IP address or name of the machine running the
   broker (Home Assistant's, for the add-on). **Port**: 1883 (8883 for TLS,
   which turns *Encrypted connection* on).
3. **User name** and **Password**: the broker user created above. A broker
   that accepts anonymous clients: *User name › None*.
4. **Name in Home Assistant**: the device's name (*Living room*, *Léa's
   Switch* …).
5. Turn **Remote link** on. The console gets a random id (8 characters, never
   the serial number), and **Status** reads *Online* within a few seconds.

Changing any of these asks for the PIN when *Security › Ask for the PIN* is
set to *Before a change*: the link can change the console.

## Keep the link up when PlayGuard is closed

The **agent** is a small optional background module (an Atmosphère
sysmodule, [`sysmodule/agent`](../sysmodule/agent/README.md)) that holds the
same link while PlayGuard is closed, with the same settings:

1. PlayGuard › **Tools › Optional modules › Remote link agent › Install**
   (PlayGuard carries it; or extract `playguard-agent.zip` from the release at
   the SD card's root). It starts at once, and at every boot.
2. Nothing else to set up: it reads *Remote access*. Its **Status** then reads
   *Online · through the agent*.
3. After an update of PlayGuard, it offers to update the agent at start-up;
   the previous agent is put back if the new one does not answer.

While PlayGuard is open, the agent stays the console's only MQTT client
(Home Assistant never sees it go offline); PlayGuard passes it what it reads
and carries out the orders, with its confirmation, PIN check and history as
usual. While PlayGuard is closed, the agent reads the console itself every
`poll_s` seconds and carries out orders **only under *Carry them out at
once***: under *Ask on the console* they wait for PlayGuard. It changes
nothing when PlayGuard was in read-only mode or after a system update
PlayGuard has not checked yet. What it changed shows up in PlayGuard's change
history, with its time, the next time PlayGuard opens.

## What appears in Home Assistant

Settings › Devices & services › MQTT: a device with the console's name,
created by MQTT discovery (turn *Create the device in Home Assistant* off to
handle the topics yourself).

| Kind | Entities |
|---|---|
| Sensors | Used screen time (the play timer's count), Used screen time (play log), Screen time remaining, Extended screen time, Now playing (empty while PlayGuard is open: nobody plays then), Age rating, Firmware, Atmosphère, Last order |
| Binary sensors | Parental controls, Temporarily unlocked, PIN set, Companion app linked, Play timer, Time is up, Network clock accurate |
| Numbers | Limit Sunday … Saturday, Limit every day, Max screentime today (0 to 1440 minutes; 1440 means no limit) |
| Time | Bedtime alarm, Bedtime end time |
| Switches | Console lock, Time's up alarm, Unlocked, Bedtime alarm enabled, VR mode restricted, Posting to social media restricted, Communication restricted |
| Selects | Restriction level, Profile (your saved profiles) |
| Buttons | Sync now, Lock now, Extra time +15 / +30 min / +1 h, No more play today, Remove the limit, Export a diagnostic report |
| Event | Events: every order applied or refused, with the reason |

The names follow Home Assistant's own *Nintendo Switch Parental Controls*
integration where they mean the same thing. The entities that change the play
timer only appear once *Orders may change the play timer* is on (below).

The play log's figures (time per game today, the last 7 days, the finished
days) are also on the broker (`playguard/<id>/activity`, `…/week`,
`…/activity/<date>`), for your own templates or a future integration.

## Orders

**Orders received** sets what PlayGuard does with an order:

- **Ask on the console** (the default): the console shows the order and asks
  before carrying it out. Nobody at the console, nothing happens; the order
  waits.
- **Carry them out at once**: no question, and no PIN prompt for the order
  (the broker already checked who sent it).
- **Refuse them**: Home Assistant only watches.

**Orders may change the play timer** is off by default. Without it, the link
can watch, lock the console again and change the restrictions, but not the
limits, bedtime or extra time. With it, an order goes exactly like a change
made on the console: when the timer counts down, PlayGuard unlocks parental
controls for a moment with the stored PIN, writes, and locks again.

Every order is in the change history (*Tools › Change history*, "Remote
link"). Some things are **never** possible through the link: deleting
parental controls, unlinking the phone app, and reading, setting or changing
the PIN. Read-only mode (a firmware PlayGuard does not know yet) refuses every
order.

## Examples

A button on a dashboard that gives 30 minutes more:

```yaml
type: button
name: +30 min
icon: mdi:timer-plus-outline
tap_action:
  action: call-service
  service: button.press
  target:
    entity_id: button.living_room_extra_time_30_min
```

The weekend limits on Friday evening:

```yaml
alias: Weekend limits
triggers:
  - trigger: time
    at: "18:00:00"
conditions:
  - condition: time
    weekday: [fri]
actions:
  - action: number.set_value
    target:
      entity_id: [number.living_room_limit_saturday, number.living_room_limit_sunday]
    data:
      value: 180
```

A notification when an order is refused:

```yaml
alias: PlayGuard order refused
triggers:
  - trigger: state
    entity_id: event.living_room_events
conditions:
  - condition: template
    value_template: "{{ trigger.to_state.attributes.event_type == 'command_rejected' }}"
actions:
  - action: notify.notify
    data:
      message: "PlayGuard refused {{ trigger.to_state.attributes.entity }}: {{ trigger.to_state.attributes.reason }}"
```

Entity ids depend on the console's name; check them on the device's page.

## Security

- Give PlayGuard **a broker user of its own**. With a plain Mosquitto, an
  access list can keep it to its topics:

  ```
  user playguard
  topic readwrite playguard/#
  topic readwrite homeassistant/device/#
  topic read homeassistant/status
  ```

- Anyone who can publish on `playguard/<id>/…/set` can send orders. Keep the
  broker on your local network, and leave *Orders may change the play timer*
  off unless you need it.
- The settings, password included, are in `sd:/switch/playguard/sync.conf`,
  in plain text. That file is never in a diagnostic report or an online
  upload; the report's *Remote link* section only has the switches and the
  counters.
- **TLS**: port 8883 and *Encrypted connection*. The console checks the
  broker's certificate against its own certificate store; for a private
  certificate authority, put its PEM file on the SD card and set
  `ca_file=/switch/playguard/sync/ca.pem` in `sync.conf`. TLS has not been
  tried on a console yet.

## When it does not connect

*Remote access › Status* says why, and *Log* shows the last lines:

| Status | What to check |
|---|---|
| *Incomplete: the broker's address is missing* | Fill in *Broker address*. |
| *Not connected: cannot find …* | The name does not resolve: use the IP address. |
| *Not connected: connect: Connection refused* / *no answer from the broker* | Wrong address or port, the broker is stopped, or a firewall is in the way. |
| *Not connected: the broker refused the connection: bad user name or password* | The user name or password. |
| *Not connected: the broker refused a subscription* | The user's access list (above). |

The console retries on its own: after 5 seconds, then twice as long each time,
up to 5 minutes. *Sync now* publishes everything again at once.

## Under the hood

The topics, documents, orders and their bounds, and every key of `sync.conf`
are in [`sync-protocol.md`](sync-protocol.md); the design and its reasons in
[`sync-design.md`](sync-design.md).
