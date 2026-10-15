const button = document.querySelector('#install');
const installer = document.querySelector('#installer');
const status = document.querySelector('#status');

async function readJson(path) {
  const response = await fetch(path, { cache: 'no-cache' });
  if (!response.ok) throw new Error(`${path}: HTTP ${response.status}`);
  return response.json();
}

async function initialize() {
  try {
    const [manifest, build] = await Promise.all([
      readJson('manifest.json'), readJson('firmware/build-info.json'),
    ]);
    if (manifest.version !== build.version || manifest.builds.length !== 1 ||
        manifest.builds[0].chipFamily !== build.chipFamily) {
      throw new Error('Die veröffentlichten Firmware-Metadaten sind inkonsistent.');
    }
    document.querySelector('#version').textContent = `v${manifest.version}`;
    document.querySelector('#chip').textContent = manifest.builds[0].chipFamily;
    document.querySelector('#flash').textContent = `${build.flashSize} · ${build.flashMode.toUpperCase()} · ${build.flashFrequency.replace('m', ' MHz')}`;
    document.querySelector('#size').textContent = `${(build.size / 1024).toLocaleString('de-DE', { maximumFractionDigits: 1 })} KiB`;
    document.querySelector('#sha').textContent = build.sha256;
    document.querySelector('#build-summary').textContent = `${build.fqbn} · Arduino ESP32 ${build.coreVersion} · ${build.library} · Partitionierung: ${build.partitionScheme} · BIN ab 0x${build.mergedOffset.toString(16).padStart(4, '0')}.`;
    const part = manifest.builds[0].parts[0];
    const firmwareUrl = new URL(part.path, new URL('manifest.json', location.href));
    document.querySelector('#download').href = firmwareUrl.href;
    await import('https://unpkg.com/esp-web-tools@10.1.1/dist/web/install-button.js?module');
    await customElements.whenDefined('esp-web-install-button');
    if (!window.isSecureContext) {
      status.textContent = 'Für die Installation ist HTTPS oder localhost erforderlich.';
    } else if (!('serial' in navigator)) {
      status.textContent = 'Bitte in Chrome oder Edge am Desktop öffnen.';
    } else {
      button.disabled = false;
      status.textContent = 'Bereit. Der USB-Port wird erst nach deinem Klick geöffnet.';
    }
  } catch (error) {
    button.disabled = true;
    status.textContent = `Installer konnte nicht geladen werden. Bitte neu laden oder die manuelle Anleitung nutzen. (${error.message})`;
  }
}

installer.addEventListener('state-changed', (event) => {
  const state = event.detail;
  if (state.state === 'finished') {
    status.textContent = 'Installiert. Verbindung schließen, am Display antippen und Touch kalibrieren.';
  } else if (state.state === 'error') {
    status.textContent = 'Installation fehlgeschlagen. Hinweise im Installationsdialog beachten.';
  }
});

initialize();
