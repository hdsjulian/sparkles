<script>
  import { onMount } from 'svelte';

  let username = '';
  let password = '';
  let error = '';
  let loading = false;

  onMount(async () => {
    // already logged in → go home
    try {
      const res = await fetch('/api/me');
      if (res.ok) window.location.href = '/';
    } catch (_) {}
  });

  async function handleSubmit(e) {
    e.preventDefault();
    error = '';
    loading = true;
    try {
      const res = await fetch('/api/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ username, password }),
      });
      const json = await res.json();
      if (!res.ok) { error = json.detail ?? 'Login failed'; return; }
      window.location.href = '/';
    } catch (e) {
      error = 'Network error';
    } finally {
      loading = false;
    }
  }
</script>

<svelte:head><title>Sparkles – Login</title></svelte:head>

<div class="login-wrap">
  <div class="login-card">
    <div class="login-logo">✦ Sparkles</div>
    <h1 class="login-title">Sign in</h1>

    {#if error}
      <div class="login-error">{error}</div>
    {/if}

    <form on:submit={handleSubmit}>
      <label class="login-label">
        Username
        <input
          class="login-input"
          type="text"
          bind:value={username}
          autocomplete="username"
          required
        />
      </label>
      <label class="login-label">
        Password
        <input
          class="login-input"
          type="password"
          bind:value={password}
          autocomplete="current-password"
          required
        />
      </label>
      <button class="btn btn-primary login-btn" type="submit" disabled={loading}>
        {loading ? 'Signing in…' : 'Sign in'}
      </button>
    </form>
  </div>
</div>

<style>
  :global(body) { background: var(--color-bg, #0d0d0d); }

  .login-wrap {
    min-height: 100vh;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 1rem;
  }

  .login-card {
    background: var(--color-surface, #1a1a1a);
    border: 1px solid var(--color-border, #2a2a2a);
    border-radius: 12px;
    padding: 2.5rem 2rem;
    width: 100%;
    max-width: 360px;
  }

  .login-logo {
    font-size: 1.5rem;
    font-weight: 700;
    color: var(--color-accent, #7c6af7);
    margin-bottom: 0.5rem;
    text-align: center;
  }

  .login-title {
    font-size: 1.1rem;
    font-weight: 600;
    color: var(--color-text, #e0e0e0);
    margin-bottom: 1.5rem;
    text-align: center;
  }

  .login-error {
    background: rgba(244, 67, 54, 0.12);
    border: 1px solid rgba(244, 67, 54, 0.4);
    color: #f44336;
    border-radius: 6px;
    padding: 0.6rem 0.9rem;
    font-size: 0.85rem;
    margin-bottom: 1rem;
  }

  .login-label {
    display: flex;
    flex-direction: column;
    gap: 0.35rem;
    font-size: 0.85rem;
    color: var(--color-text-muted, #888);
    margin-bottom: 1rem;
  }

  .login-input {
    background: var(--color-bg, #0d0d0d);
    border: 1px solid var(--color-border, #2a2a2a);
    border-radius: 6px;
    padding: 0.55rem 0.75rem;
    color: var(--color-text, #e0e0e0);
    font-size: 0.95rem;
    outline: none;
    transition: border-color 0.15s;
  }

  .login-input:focus {
    border-color: var(--color-accent, #7c6af7);
  }

  .login-btn {
    width: 100%;
    margin-top: 0.5rem;
    padding: 0.65rem;
    font-size: 0.95rem;
  }
</style>
