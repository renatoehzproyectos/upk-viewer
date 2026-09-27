# Cambio pendiente en tu repositorio

`viewer3d/web/index.html` ya está creado (visor 3D de prueba con three.js).

Falta un cambio que no se puede aplicar aquí porque `www/` y
`.github/workflows/deploy.yml` no venían en el archivo enviado.

En `.github/workflows/deploy.yml`, en el paso "Assemble site", reemplaza:

```yaml
      - name: Assemble site
        run: |
          mkdir -p _site
          cp -r www/* _site/
```

por:

```yaml
      - name: Assemble site
        run: |
          mkdir -p _site
          cp -r www/* _site/
          mkdir -p _site/viewer3d
          cp -r viewer3d/web/* _site/viewer3d/
```

Opcional, en `www/index.html` cerca del `<header>`:

```html
<p><a href="./viewer3d/">Probar el nuevo visor 3D (en desarrollo)</a></p>
```
