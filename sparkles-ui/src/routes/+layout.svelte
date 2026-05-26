<script>
  import { onMount, onDestroy } from 'svelte';
  import Nav from '$lib/components/Nav.svelte';
  import { setupSSE } from '$lib/sse.js';
  import '../app.css';

  let cleanupSSE;

  onMount(() => {
    cleanupSSE = setupSSE();
  });

  onDestroy(() => {
    if (cleanupSSE) cleanupSSE();
  });
</script>

<Nav />

<div class="page-wrapper">
  <slot />
</div>
