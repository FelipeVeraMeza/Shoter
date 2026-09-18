# Plataforma de Gestión de Envíos — documento de alcance

Esta carpeta **no forma parte del juego Shoter**. Contiene el documento de alcance y el
cuestionario para el cliente de una aplicación aparte: gestión de envíos con sesión,
dirección, producto, foto, comuna, ticket con QR, apertura de mapas y registro de
ganancias.

| Archivo | Qué es |
|---------|--------|
| `ESPECIFICACION_APP_ENVIOS.pdf` | El documento para enviar al cliente (28 páginas) |
| `especificacion_app_envios.html` | La fuente del PDF; se edita este archivo |

## Regenerar el PDF

```bash
pip install weasyprint
cd docs/envios
python3 -c "from weasyprint import HTML; HTML('especificacion_app_envios.html').write_pdf('ESPECIFICACION_APP_ENVIOS.pdf')"
```

## Estado

Versión 1.0 — borrador. El alcance **no está cerrado**: depende de las respuestas del
cuestionario (sección 13 del PDF), en especial de la pregunta D-0 (si «mps» es Maps o
Mercado Pago) y del bloque A.
