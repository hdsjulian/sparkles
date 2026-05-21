const NAV_LINKS = [
  { href: 'addressList.html',   label: 'Address List' },
  { href: 'newCalibration.html',label: 'Calibration' },
  { href: 'settings.html',      label: 'Settings' },
  { href: 'animations.html',    label: 'Animations' },
  { href: 'midiSettings.html',  label: 'Midi Settings' },
  { href: 'darkroom.html',      label: 'Darkroom' },
  { href: 'battery.html',       label: 'Battery' },
];

function toggleMenu() {
  const menu = document.getElementById('navMenu');
  if (!menu) return;
  menu.style.display = menu.style.display === 'flex' ? 'none' : 'flex';
}

function buildNav() {
  const menu = document.getElementById('navMenu');
  if (!menu) return;
  const current = location.pathname.split('/').pop();
  menu.innerHTML = NAV_LINKS.map(({ href, label }) =>
    `<a href="${href}"${href === current ? ' class="active"' : ''}>${label}</a>`
  ).join('');
}

document.addEventListener('DOMContentLoaded', buildNav);
