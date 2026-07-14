<script>
  import { page } from '$app/stores';
  import { createEventDispatcher } from 'svelte';

  export let authUser = null;  // { username, role, allowedPages }

  const dispatch = createEventDispatcher();
  let menuOpen = false;

  const allLinks = [
    { href: '/',            label: 'Dashboard',   icon: '⊞', roles: ['admin', 'user'] },
    { href: '/battery',     label: 'Battery',      icon: '🔋', roles: ['admin', 'user'] },
    { href: '/settings',    label: 'Settings',     icon: '⚙',  roles: ['admin'] },
    { href: '/animations',  label: 'Animations',   icon: '✨', roles: ['admin', 'user'] },
    { href: '/midi',        label: 'MIDI',         icon: '🎵', roles: ['admin', 'user'] },
    { href: '/darkroom',    label: 'Darkroom',     icon: '🌑', roles: ['admin', 'user'] },
    { href: '/calibration', label: 'Calibration',  icon: '📐', roles: ['admin'] },
    { href: '/log',         label: 'Serial Log',   icon: '📋', roles: ['admin'] },
    { href: '/timertest',   label: 'Timer Test',   icon: '⏱',  roles: ['admin'] },
    { href: '/sleeptest',   label: 'Sleep Test',   icon: '💤', roles: ['admin'] },
  ];

  $: role = authUser?.role ?? 'user';
  $: links = allLinks.filter(l => l.roles.includes(role));

  function toggle() { menuOpen = !menuOpen; }
  function close()  { menuOpen = false; }
</script>

<header class="topnav">
  <button class="hamburger" class:open={menuOpen} on:click={toggle} aria-label="Toggle navigation">
    <span></span>
    <span></span>
    <span></span>
  </button>
  <span class="brand">✦ Sparkles</span>
  {#if authUser}
    <span class="nav-user">{authUser.username}</span>
  {/if}
</header>

<!-- Overlay -->
<div
  class="nav-overlay"
  class:visible={menuOpen}
  on:click={close}
  on:keydown={(e) => e.key === 'Escape' && close()}
  role="presentation"
></div>

<!-- Side drawer -->
<div class="nav-menu" class:open={menuOpen}>
  <nav>
    {#each links as link}
      <a
        href={link.href}
        class:active={$page.url.pathname === link.href}
        on:click={close}
      >
        <span class="nav-icon">{link.icon}</span>
        {link.label}
      </a>
    {/each}
  </nav>

  {#if authUser}
    <div class="nav-footer">
      <button class="btn btn-ghost btn-sm logout-btn" on:click={() => { close(); dispatch('logout'); }}>
        Sign out
      </button>
    </div>
  {/if}
</div>

<style>
  .nav-user {
    margin-left: auto;
    margin-right: 1rem;
    font-size: 0.78rem;
    color: var(--color-text-muted);
  }

  .nav-footer {
    margin-top: auto;
    padding: 1rem;
    border-top: 1px solid var(--color-border, #2a2a2a);
  }

  .logout-btn {
    width: 100%;
    justify-content: center;
  }
</style>
