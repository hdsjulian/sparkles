<script>
  import { onMount, onDestroy } from 'svelte';
  import { page } from '$app/stores';
  import Nav from '$lib/components/Nav.svelte';
  import { setupSSE } from '$lib/sse.js';
  import '../app.css';

  let cleanupSSE;
  let authUser = null;
  let authChecked = false;
  let serialConnected = true;
  let serialStale = false;
  let serialEs;
  let serialPollInterval;

  $: isLoginPage = $page.url.pathname === '/login';

  onMount(async () => {
    if (isLoginPage) { authChecked = true; return; }

    // Tell other tabs to close their SSE connections
    const bc = new BroadcastChannel('sparkles_tab');
    bc.postMessage('claim');
    bc.onmessage = (e) => {
      if (e.data === 'claim') {
        if (cleanupSSE) { cleanupSSE(); cleanupSSE = null; }
        if (serialEs) { serialEs.close(); serialEs = null; }
      }
    };

    try {
      const res = await fetch('/api/me');
      if (res.ok) {
        authUser = await res.json();
        cleanupSSE = setupSSE();
        initSerialStatus();
      } else {
        window.location.href = '/login';
      }
    } catch (_) {
      window.location.href = '/login';
    }
    authChecked = true;
  });

  function pollSerialStatus() {
    fetch('/serial-status').then(r => r.json()).then(d => {
      serialConnected = d.connected;
      serialStale = d.stale ?? false;
    }).catch(() => {});
  }

  function initSerialStatus() {
    pollSerialStatus();
    serialPollInterval = setInterval(pollSerialStatus, 15000);

    // live disconnect/reconnect events via SSE
    serialEs = new EventSource('/events');
    serialEs.addEventListener('serial_status', (e) => {
      const d = JSON.parse(e.data);
      serialConnected = d.connected;
      if (d.stale !== undefined) serialStale = d.stale;
      else if (d.connected) pollSerialStatus(); // fallback: refresh stale on reconnect
    });
  }

  onDestroy(() => {
    if (cleanupSSE) cleanupSSE();
    if (serialEs) serialEs.close();
    if (serialPollInterval) clearInterval(serialPollInterval);
  });

  async function logout() {
    await fetch('/api/logout', { method: 'POST' });
    window.location.href = '/login';
  }
</script>

{#if isLoginPage}
  <slot />
{:else if authChecked && authUser}
  <Nav {authUser} on:logout={logout} />
  {#if !serialConnected}
    <div class="serial-warning">
      ⚠ No serial connection to master — commands will not reach the device
    </div>
  {:else if serialStale}
    <div class="serial-warning">
      ⚠ Master is connected but not responding — it may be hung
    </div>
  {/if}
  <div class="page-wrapper" class:has-warning={!serialConnected || serialStale}>
    <slot />
  </div>
{:else if !authChecked}
  <!-- waiting for auth check -->
{/if}

<style>
  .serial-warning {
    position: fixed;
    top: 56px;
    left: 0;
    right: 0;
    z-index: 199;
    background: rgba(244, 152, 0, 0.15);
    border-bottom: 1px solid rgba(244, 152, 0, 0.5);
    color: #f98000;
    font-size: 0.82rem;
    font-weight: 600;
    padding: 0.5rem 1.25rem;
    text-align: center;
    letter-spacing: 0.02em;
  }

  :global(.page-wrapper.has-warning) {
    padding-top: 91px; /* 56px nav + 35px warning */
  }
</style>
