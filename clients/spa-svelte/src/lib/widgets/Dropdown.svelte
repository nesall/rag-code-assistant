<script lang="ts">
  import * as icons from "@lucide/svelte";
  import { onMount } from "svelte";
  import { initResizeObserver, isPtInRect, nextRandomId } from "../utils";
  import { autoUpdate, computePosition, flip, offset, shift } from "@floating-ui/dom";
  import { fade } from "svelte/transition";

  interface Props {
    value?: string;
    values?: Array<string | { value: string; label: string; desc?: string; hint?: string }>;
    id?: string;
    classNames?: string;
    onChange?: (i: number, value: string) => void;
  }

  let {
    value = $bindable(""),
    values = [],
    id = nextRandomId(4),
    classNames = "preset-outlined-surface-500",
    onChange = (i: number, value: string) => {},
  }: Props = $props();

  function valueStr(v: string | { value: string; label: string }) {
    if (typeof v === "string") return v;
    return v.value;
  }

  function labelStr(v: string | { value: string; label: string }) {
    if (typeof v === "string") return v;
    return v.label;
  }

  function descStr(v: string | { value: string; label: string; desc?: string }) {
    if (typeof v === "string") return "";
    return v.desc;
  }

  function hintStr(v: string | { value: string; label: string; hint?: string }) {
    if (typeof v === "string") return "";
    return v.hint;
  }

  let delAutoUpdate: (() => void) | undefined;
  function recomputePosition() {
    if (!elemAnchor || !elemFloat) return;
    delAutoUpdate = autoUpdate(elemAnchor, elemFloat, () => {
      if (elemFloat && elemAnchor)
        computePosition(elemAnchor, elemFloat, {
          strategy: "fixed",
          placement: "bottom",
          middleware: [offset(4), flip(), shift()],
        }).then(({ x, y }) => {
          if (elemFloat)
            Object.assign(elemFloat.style, {
              left: `${x}px`,
              top: `${y}px`,
            });
        });
    });
  }

  onMount(() => {
    if (elemAnchor)
      initResizeObserver(elemAnchor, () => {
        if (delAutoUpdate) delAutoUpdate();
        if (elemAnchor) {
          anchorWidth = elemAnchor.offsetWidth;
          recomputePosition();
        }
      });
    return () => {
      if (delAutoUpdate) delAutoUpdate();
    };
  });

  $effect(() => {
    if (show && elemFloat && elemAnchor) {
      // Wait for DOM to settle
      requestAnimationFrame(() => {
        recomputePosition();
      });
    } else if (!show && delAutoUpdate) {
      delAutoUpdate();
      delAutoUpdate = undefined;
    }
  });

  const currentLabel = $derived.by(() => {
    for (let v of values) {
      if (valueStr(v) === value) {
        return labelStr(v);
      }
    }
    return value;
  });

  const currentDesc = $derived.by(() => {
    for (let v of values) {
      if (valueStr(v) === value) {
        return descStr(v);
      }
    }
    return "";
  });

  let show = $state(false);

  let currentHint: string | undefined = $state();

  let elemFloat: HTMLElement | undefined = $state();
  let elemAnchor: HTMLElement | undefined = $state();
  let anchorWidth = $state(10);

  function onItemClick(i: number, v: string) {
    value = v;
    currentHint = hintStr(values[i]);
    onChange(i, v);
    setTimeout(() => (show = false), 100);
  }

  function onClick() {
    show = !show;
  }

  function windowPressed(e: MouseEvent) {
    if (
      elemFloat &&
      elemAnchor &&
      !isPtInRect(elemFloat.getBoundingClientRect(), e.clientX, e.clientY) &&
      !isPtInRect(elemAnchor.getBoundingClientRect(), e.clientX, e.clientY)
    ) {
      show = false;
    }
  }
  function windowKeyDown(e: KeyboardEvent) {
    switch (e.key) {
      case "Escape":
        show = false;
        break;
    }
  }
</script>

<svelte:window onmousedown={windowPressed} onkeydown={windowKeyDown} />

<div
  class="combobox relative inline"
  role="combobox"
  aria-expanded={show}
  aria-controls="{id}-combobox"
  aria-haspopup="listbox"
  aria-label="Select an option"
  bind:this={elemAnchor}
>
  <button
    type="button"
    class="combobox-btn w-full flex items-center rounded px-2 py-1 {classNames}"
    onclick={onClick}
    title={currentHint}
  >
    <div class="flex-1 text-left items-center flex space-x-4">
      <span class="text-left">
        {currentLabel}
      </span>
      {#if currentDesc}
        <span class="text-surface-500">
          {currentDesc}
        </span>
      {/if}
    </div>
    {#if show}
      <icons.ChevronUp size={16} />
    {:else}
      <icons.ChevronDown size={16} />
    {/if}
  </button>
  {#if show}
    <div
      id="{id}-dropdown-list"
      style="position: fixed; width: {anchorWidth}px; z-index: 1000;"
      class="dropdown-list rounded shadow flex flex-col max-h-48 overflow-y-auto absolute2"
      role="listbox"
      bind:this={elemFloat}
      transition:fade={{ duration: 100 }}
    >
      {#each values as item, i (valueStr(item))}
        <button
          data-value={valueStr(item)}
          class="bg-surface-50-950 hover:bg-surface-100-900 text-left px-3 py-1 w-full
                flex items-center justify-between
                {valueStr(item) === value ? 'font-bold' : ''}"
          role="option"
          aria-selected={valueStr(item) === value}
          onclick={() => onItemClick(i, valueStr(item))}
          title={hintStr(item)}
        >
          {labelStr(item)}
          {#if descStr(item)}
            <span class="text-surface-500 text-right">
              {descStr(item)}
            </span>
          {/if}
        </button>
      {/each}
    </div>
  {/if}
</div>
