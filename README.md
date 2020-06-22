# EnigmaIOT-Blind-Controller

Actual blind controller node based on EnigmaIOT library. Used to program [Loratap SC511WSC](https://www.loratap.com/sc500w-v2-p0108.html) module or similar ESP8266 based blind controllers.

![](https://www.loratap.com/u_file/2003/products/25/47ef90344e.jpg.640x640.jpg)

This may be used for blinds, curtains, shades and equivalent devices.

Using a Gateway loaded with [EnigmaIOTGatewayMQTT](https://github.com/gmag11/EnigmaIOT/tree/master/examples/EnigmaIOTGatewayMQTT) example it uses MQTT as interface to send messages and receive commands. Besides standard EnigmaIOT messages and commands this firmware implements these custom ones.

## Messages

#### Button actions

Sends a message on every button press and tells about how many times it has been quickly pressed

```
<Network name>/<node name>|<node address>/data {"cmd":"event","but":<"up"|"down">,"num":<number of presses>}
```

**Topic**													**Payload**

`EnigmaIOT/room_bilnd/data`		`{"cmd":"event","but":"up","num":2}`  ---> Button **up** has been pressed **twice**

#### Blind position

Blind position is sent regularly every some minutes. During movement it is also sent every some seconds.

```
<Network name>/<node name>|<node address>/data {"state":<state number>,"pos":<blind position>}
```

| State number | Meaning      |
| ------------ | ------------ |
| 1            | Rolling up   |
| 2            | Rolling Down |
| 4            | Stopped      |
| 0            | Error        |



| Blind position | Meaning                              |
| -------------- | ------------------------------------ |
| 0              | Fully closed                         |
| 100            | Fully open                           |
| -1             | Unknown position. Not yet calibrated |

Blind position is calibrated on every full open or full close command. Until first full movement command after botting up position is signaled as unknown.

**Topic**													**Payload**

`EnigmaIOT/room_bilnd/data`		`{"state":4,"pos":100}`  ---> Blind **stopped** at **fully open** position

## Commands

### Get blind position

Ask blind controller to send blind position inmediatelly.

```
<Network name>/<node name>|<node address>/data/get {"cmd":"pos"}
```

**Topic**															**Payload**

`EnigmaIOT/room_bilnd/data`/get		`{"cmd":"pos"}` 

#### Response

`EnigmaIOT/room_blind/data {"cmd":"pos","pos":100}` --->  Blind fully open

### Get full roll time

Gets configured time required to move blind between extreme positions.

```
<Network name>/<node name>|<node address>/data/get {"cmd":"time"}
```

**Topic**															**Payload**

`EnigmaIOT/room_bilnd/data`/get		`{"cmd":"time"}` 

#### Response

`EnigmaIOT/room_blind/data {"cmd":"time","time":30}` --->  Full blind movement time is configured as 30 seconds

### Set full roll time

Gets configured time required to move blind between extreme positions.

```
<Network name>/<node name>|<node address>/data/set {"cmd":"time","time":<Full movement time in seconds>}
```

**Topic**															**Payload**

`EnigmaIOT/room_bilnd/data`/set		`{"cmd":"time","time":20}`  ---> Set full blind movement time to 20 seconds.

#### Response

`EnigmaIOT/room_blind/data {"cmd":"time","time":20}` --->  Full blind movement is configured as 20 seconds

### Fully roll up blind

Triggers a full roll up movement.

```
<Network name>/<node name>|<node address>/data/set {"cmd":"uu"}
```

**Topic**															**Payload**

`EnigmaIOT/room_bilnd/data`/set		`{"cmd":"uu"}` 

#### Respose

`EnigmaIOT/room_blind/data {"cmd":"uu","result":1}` --->  UU command executed successfully. 

Result:`1` = Ok, `0` = Not ok

### Fully roll down blind

Triggers a full roll down movement.

```
<Network name>/<node name>|<node address>/data/set {"cmd":"dd"}
```

**Topic**															**Payload**

`EnigmaIOT/room_bilnd/data`/set		`{"cmd":"dd"}` 

#### Respose

`EnigmaIOT/room_blind/data {"cmd":"dd","result":1}` --->  UU command executed successfully. 

Result:`1` = Ok, `0` = Not ok

### Stop blind

Stops blind unconditionally.

```
<Network name>/<node name>|<node address>/data/set {"cmd":"stop"}
```

**Topic**															**Payload**

`EnigmaIOT/room_bilnd/data`/set		`{"cmd":"stop"}` 

#### Respose

`EnigmaIOT/room_blind/data {"cmd":"stop","result":1}` --->  UU command executed successfully. 

Result:`1` = Ok, `0` = Not ok

### Move blind to position

Move blind to an arbitrary position between 0 (closed) and 100 (open). Values greater than 100 are interpreted as 100. Values lower than 0 are interpreted as 0.

**Move to position 100** is equivalent to **full roll up** and **move to position 0** is equivalent to **full roll down**.

```
<Network name>/<node name>|<node address>/data/set {"cmd":"go","pos":<New position>}
```

**Topic**															**Payload**

`EnigmaIOT/room_bilnd/data`/set		`{"cmd":"go","pos":50}` ---> Move blind to mid position.

#### Respose

`EnigmaIOT/room_blind/data {"cmd":"go","result":1}` --->  UU command executed successfully. 

Result:`1` = Ok, `0` = Not ok

