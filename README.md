# ESP32 Car HTTP + MQTT: Guía de uso con Postman y Mosquitto

Este documento explica **cómo controlar el carro desde Postman** y cómo **verificar las publicaciones MQTT** en `test.mosquitto.org` usando `mosquitto_sub`.

---

- Link Chatgpt: [https://chatgpt.com/share/68e6ef12-c018-8007-91c2-bfa94164f942] 

## 1. Requisitos

- ESP32 conectado al WiFi (en el Monitor Serie debe mostrar algo como):
  ```
  WiFi connected, IP: 172.20.44.194
  ```
- Postman instalado → [https://www.postman.com/downloads](https://www.postman.com/downloads)
- Cliente Mosquitto instalado:
  - macOS: `brew install mosquitto`
  - Linux: `sudo apt install mosquitto-clients`
  - Windows: [https://mosquitto.org/download/](https://mosquitto.org/download/)

  - Descargar y pasar el código llamado mqtt1 a la ESP32
  - descargar el archivo .jsn en PostMan
---

## 2. Configurar Postman

1. Abre Postman → **Import** → selecciona el archivo `docs/postman_collection.json` del proyecto.
   ![eje plo](a.png)
   * *
   *Ejemplo de cómo debe quedar*
3. Crea un **Environment** (entorno):
   - Nombre: `ESP32-Car`
   - Variable: `esp32_ip`
   - Valor: la IP de tu ESP32 (ejemplo: `172.20.44.194`)
4. Activa el environment desde la parte superior derecha.

---

## 3. Probar endpoints

En la colección **ESP32 Car HTTP + MQTT**:
![getHealth](Health.png)
### `GET /health`
Verifica conexión y estado:
```json
{
  "status": "ok",
  "wifi_ip": "172.20.44.194",
  "mqtt_connected": false,
  "motion_active": false
}
```
![postmove](MoveF.png)
### `POST /api/move`
Envía una instrucción de movimiento con parámetros:
- `direction`: `forward|backward|left|right|stop`
- `speed`: 0–100
- `duration_ms`: hasta 5000

Ejemplo:
```bash
curl "http://172.20.44.194/api/move?direction=forward&speed=60&duration_ms=2000"
```
Respuesta esperada:
```json
{
  "status": "accepted",
  "direction": "forward",
  "speed": 60,
  "duration_ms": 2000,
  "client_ip": "172.20.44.100"
}
```
![TurnLeft](TurnL.png)
*Turn Left*
![Stop](Stop.png)
*Stop*
---

## 4. Verificar MQTT con Mosquitto

Abre una terminal nueva y ejecuta:

```bash
mosquitto_sub -h test.mosquitto.org -t esp32car/commands -v
```

Esto **escucha los mensajes publicados por el ESP32** cuando recibe órdenes HTTP.

Ejemplo de salida:
```
esp32car/commands {"status":"accepted","direction":"forward","speed":60,"duration_ms":2000,"client_ip":"172.20.44.100","ts":345678}
```

---

## 5. ¿Por qué usamos `mosquitto_sub`?

-  **Verificación:** demuestra que el ESP32 publica las instrucciones por MQTT (parte del requisito del proyecto).  
-  **Auditoría:** permite ver cada mensaje JSON en tiempo real.  
-  **Diagnóstico:** si no ves mensajes, sabes que el problema está en la conexión MQTT o en que la orden no fue aceptada.

> El carro se controla 100% por HTTP, pero usamos `mosquitto_sub` para **comprobar** que las publicaciones MQTT están funcionando.

---

## 6. Problemas comunes

| Problema | Causa | Solución |
|-----------|--------|----------|
| `/health` muestra `mqtt_connected: false` | El ESP32 no logra conectar al broker | La red puede bloquear el puerto 1883; prueba con otra red o un hotspot móvil |
| No ves mensajes en Mosquitto | Estás en otro topic o la orden fue rechazada | Usa exactamente `esp32car/commands` y verifica que `/api/move` devuelve 202 |
| El carro no se mueve | Cableado incorrecto o inversión de pines | Revisa los pines del L298N, usa movimientos cortos de prueba |
| El broker está saturado | `test.mosquitto.org` es público | Cambia a otro, como `broker.hivemq.com` |

---

## 7. Ejemplo completo de prueba

1. En una terminal:  
   ```bash
   mosquitto_sub -h test.mosquitto.org -t esp32car/commands -v
   ```

2. En Postman o terminal:
   ```bash
   curl "http://172.20.44.194/api/move?direction=forward&speed=60&duration_ms=2000"
   ```

3. Verás el movimiento físico y, en la terminal del paso 1, un mensaje publicado por MQTT.

---

 **Listo:** con esto puedes controlar el carro desde Postman y verificar en Mosquitto que las instrucciones se publican correctamente.
