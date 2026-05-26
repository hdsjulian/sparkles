<script>
  import { onMount, onDestroy, createEventDispatcher } from 'svelte';
  import noUiSlider from 'nouislider';

  export let min = 0;
  export let max = 100;
  export let start = [0, 100]; // array for range, single value for single
  export let step = 1;
  export let connect = true;
  export let tooltips = true;
  export let disabled = false;

  const dispatch = createEventDispatcher();

  let el;
  let slider;

  onMount(() => {
    const startArr = Array.isArray(start) ? start : [start];
    const connectVal = Array.isArray(start)
      ? connect
      : [false, connect === true ? true : false, false];

    slider = noUiSlider.create(el, {
      start: startArr,
      connect: Array.isArray(connect) ? connect : (startArr.length === 1 ? [connect, false] : connect),
      range: { min, max },
      step,
      tooltips: tooltips ? startArr.map(() => true) : false,
    });

    slider.on('change', (values) => {
      const parsed = values.map(v => parseFloat(v));
      dispatch('change', Array.isArray(start) ? parsed : parsed[0]);
    });

    if (disabled) slider.disable();
  });

  // Allow external update of start values
  export function setValues(vals) {
    if (slider) {
      slider.set(Array.isArray(vals) ? vals : [vals]);
    }
  }

  // Enable/disable reactively
  $: if (slider) {
    if (disabled) {
      slider.disable();
    } else {
      slider.enable();
    }
  }

  onDestroy(() => {
    if (slider) slider.destroy();
  });
</script>

<div class="nouislider-wrap">
  <div bind:this={el}></div>
</div>
