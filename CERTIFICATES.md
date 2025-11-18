**Explicación completa sobre TLS, certificados y su uso en ESP32**

## 1. ¿Qué es el protocolo TLS, cuál es su importancia y qué es un certificado en ese contexto?

TLS (*Transport Layer Security*) es un protocolo criptográfico que protege las comunicaciones en internet mediante **cifrado**, **integridad** y **autenticación**.
Su importancia radica en que:

* Evita que terceros lean los datos (confidencialidad).
* Evita que los datos sean modificados (integridad).
* Permite verificar que se está hablando con el servidor legítimo (autenticación).

Un **certificado digital** es un documento firmado que asocia la identidad de un servidor con una clave pública. Es lo que permite verificar que un servidor es quien dice ser.

---

## 2. ¿A qué riesgos se está expuesto si no se usa TLS?

Sin TLS, la comunicación se transmite **en texto plano**, lo que expone a:

* **Sniffing**: atacantes pueden leer usuarios, contraseñas o datos privados.
* **Man-in-the-Middle (MITM)**: un atacante puede interceptar y modificar mensajes.
* **Suplantación de servidores**: un atacante puede hacerse pasar por el servidor legítimo.
* **Inyección de comandos o telemetría falsa** especialmente crítica en IoT.

---

## 3. ¿Qué es un CA (Certificate Authority)?

Una **Certificate Authority** es una entidad de confianza responsable de emitir certificados digitales firmados.
Ejemplos: Let’s Encrypt, DigiCert, GlobalSign.

Los navegadores y dispositivos mantienen una lista de CAs confiables.

---

## 4. ¿Qué es una cadena de certificados y cuál es la vigencia promedio de sus eslabones?

La cadena de certificados (certificate chain) es la relación jerárquica:

1. **Root CA (autoridad raíz)**
2. **Intermediate CA (intermediarios)**
3. **Certificado del servidor**

El ESP32 necesita conocer al menos el *Root CA* o el *Intermediate* para validar un servidor.

**Vigencia típica:**

* Root CA → *15 a 25 años*
* Intermediate CA → *3 a 5 años*
* Certificado del servidor → *3 meses a 1 año*

---

## 5. ¿Qué es un keystore y qué es un certificate bundle?

* **Keystore**: archivo que almacena claves privadas y certificados para aplicaciones o servidores (ej.: Java KeyStore `.jks`, PKCS#12 `.p12`).
* **Certificate bundle**: archivo que contiene varias CAs juntas para validación (ej.: `ca-bundle.crt` que usa Linux).

---

## 6. ¿Qué es la autenticación mutua en el contexto de TLS?

En la autenticación mutua (mTLS):

* El **cliente verifica al servidor**, y
* El **servidor también verifica al cliente**.

Ambos presentan certificados válidos.
Esto se usa en sistemas de alta seguridad, APIs privadas y dispositivos IoT críticos.

---

## 7. ¿Cómo se habilita la validación de certificados en el ESP32?

En Arduino para ESP32, la validación se habilita asignando la CA con:

```cpp
WiFiClientSecure client;
client.setCACert(root_ca);
```

Sin este paso, el ESP32 permite conexiones inseguras (como si se usara `setInsecure()`).

---

## 8. Si el sketch necesita conectarse a múltiples dominios con certificados generados por CAs distintos, ¿qué alternativas hay?

Opciones:

1. **Concatenar múltiples certificados** en un solo `root_ca.h`.
2. **Usar setCACert() dinámicamente** según el servidor al que se conecte.
3. **Cargar el certificate bundle completo** (solo posible si cabe en memoria).
4. **Deshabilitar validación (inseguro)** → *NO recomendado*.

---

## 9. ¿Cómo se puede obtener el certificado para un dominio?

Desde terminal:

```bash
openssl s_client -showcerts -connect dominio:443 </dev/null
```

Luego copiar manualmente el certificado `-----BEGIN CERTIFICATE----- …`.

O descargarlo desde el navegador (Chrome → candado → detalles → exportar).

---

## 10. ¿A qué se hace referencia cuando se habla de llave pública y privada en el contexto de TLS?

TLS usa criptografía asimétrica:

* **Llave pública** → se comparte; el cliente la usa para verificar al servidor o cifrar datos iniciales.
* **Llave privada** → solo la posee el servidor; se usa para descifrar y demostrar identidad.

Son dos partes matemáticas de un mismo par de claves.

---

## 11. ¿Qué pasará con el código cuando los certificados expiren?

Cuando el certificado expira:

* **El ESP32 rechazará la conexión**.
* `mqttClient.connect()` o `client.connect()` fallarán.
* Se debe **actualizar el certificado en el firmware** y reprogramar el ESP32.

Por eso muchas aplicaciones IoT migran a CAs con validez larga o usan mecanismos automáticos de actualización OTA.

---

## 12. ¿Qué teoría matemática es el fundamento de la criptografía moderna?

Los fundamentos principales son:

* **Teoría de números**
* **Aritmética modular**
* **Problemas difíciles computacionalmente** como:

  * Factorización de enteros grandes (RSA)
  * Logaritmo discreto (DH, DSA)
  * Curvas elípticas (ECC)

### ¿Y la computación cuántica?

Los computadores cuánticos podrían romper:

* RSA
* DH
* ECC

usando el **algoritmo de Shor**, haciendo factorización y logaritmos discretos casi instantáneos.

Esto lleva al movimiento hacia **criptografía post-cuántica (PQC)**: algoritmos resistentes a ataques cuánticos
