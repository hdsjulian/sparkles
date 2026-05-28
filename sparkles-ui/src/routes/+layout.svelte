<script>
  import { onMount, onDestroy } from 'svelte';
  import { page } from '$app/stores';
  import Nav from '$lib/components/Nav.svelte';
  import { setupSSE } from '$lib/sse.js';
  import '../app.css';

  let cleanupSSE;
  let authUser = null;   // { username, role, allowedPages }
  let authChecked = false;

  $: isLoginPage = $page.url.pathname === '/login';

  onMount(async () => {
    if (isLoginPage) { authChecked = true; return; }

    try {
      const res = await fetch('/api/me');
      if (res.ok) {
        authUser = await res.json();
        cleanupSSE = setupSSE();
      } else {
        window.location.href = '/login';
      }
    } catch (_) {
      window.location.href = '/login';
    }
    authChecked = true;
  });

  onDestroy(() => {
    if (cleanupSSE) cleanupSSE();
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
  <div class="page-wrapper">
    <slot />
  </div>
{:else if !authChecked}
  <!-- waiting for auth check -->
{/if}
