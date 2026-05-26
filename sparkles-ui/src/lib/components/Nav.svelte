<script>
  import { page } from '$app/stores';

  let menuOpen = false;

  const links = [
    { href: '/',            label: 'Dashboard',   icon: '⊞' },
    { href: '/battery',     label: 'Battery',      icon: '🔋' },
    { href: '/settings',    label: 'Settings',     icon: '⚙' },
    { href: '/animations',  label: 'Animations',   icon: '✨' },
    { href: '/midi',        label: 'MIDI',         icon: '🎵' },
    { href: '/darkroom',    label: 'Darkroom',     icon: '🌑' },
    { href: '/calibration', label: 'Calibration',  icon: '📐' },
  ];

  function toggle() {
    menuOpen = !menuOpen;
  }

  function close() {
    menuOpen = false;
  }
</script>

<header class="topnav">
  <button class="hamburger" class:open={menuOpen} on:click={toggle} aria-label="Toggle navigation">
    <span></span>
    <span></span>
    <span></span>
  </button>
  <span class="brand">✦ Sparkles</span>
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
</div>
