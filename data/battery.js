'use strict';
import { toggleMenu, fetchData } from './sparkles.js';

const devices = {};

function batteryClass(pct) {
  if (pct <= 20) return 'battery-low';
  if (pct <= 50) return 'battery-mid';
  return 'battery-ok';
}

function render() {
  const sorted = Object.values(devices).sort((a, b) => a.batteryPercentage - b.batteryPercentage);
  const tbody = document.getElementById('batteryBody');
  tbody.innerHTML = '';
  for (const dev of sorted) {
    const tr = document.createElement('tr');
    if (dev.status !== 'active') tr.className = 'inactive';
    const cls = batteryClass(dev.batteryPercentage);
    tr.innerHTML = `
      <td>Device ${dev.id}</td>
      <td class="${cls}">${dev.batteryPercentage}%</td>
      <td><button data-id="${dev.id}">Blink</button></td>`;
    tbody.appendChild(tr);
  }
  tbody.querySelectorAll('button[data-id]').forEach(btn => {
    btn.addEventListener('click', () => {
      fetchData(`/commandBlink?boardId=${btn.dataset.id}`);
    });
  });
}

function upsert(obj) {
  devices[obj.id] = { ...devices[obj.id], ...obj };
  render();
}

function setupEventSource() {
  if (!window.EventSource) return;
  const src = new EventSource('/events');
  src.addEventListener('new_board',    e => upsert(JSON.parse(e.data)));
  src.addEventListener('update_board', e => upsert(JSON.parse(e.data)));
}

document.addEventListener('DOMContentLoaded', () => {
  document.getElementById('hamburgerMenu').addEventListener('click', toggleMenu);

  fetch('/getAddressList')
    .then(r => r.json())
    .then(data => { data.addresses.forEach(upsert); })
    .catch(console.error);

  setupEventSource();
});
