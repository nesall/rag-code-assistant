<script lang="ts">
  import * as icons from "@lucide/svelte";
  import Checkbox from "./Checkbox.svelte";
  import { Modal } from "@skeletonlabs/skeleton-svelte";
  import { onMount } from "svelte";
  import { bytesToSize, hash } from "../utils";
  import { fade, fly } from "svelte/transition";

  interface Props {
    loading: boolean;
    onChange: (files: string[]) => void;
  }
  let { loading = false, onChange }: Props = $props();

  let openState = $state(false);
  let filterValue = $state("");

  interface Document {
    path: string;
    size: number;
    lastModified: number;
    _visible?: boolean;
    _name: string;
    _checked: boolean;
  }
  let documents: Document[] = $state([]);

  const filteredDocs = $derived(
    documents.filter((d) => d.path.includes(filterValue)),
  );

  onMount(() => {
    const saved = JSON.parse(sessionStorage.getItem("contextFiles") || "[]");
    fetch("/api/documents")
      .then((response) => response.json())
      .then((data) => {
        documents = data.map((doc: any) => ({
          ...doc,
          _name: doc.path.split("/").pop() || doc.path,
          _visible:
            saved.find((d: any) => d.path === doc.path)?._visible || false,
          _checked:
            saved.find((d: any) => d.path === doc.path)?._checked || false,
        }));
        onChange(documents.filter((d) => d._checked).map((d) => d.path));
      })
      .catch((error) => {
        console.error("Error fetching context files:", error);
      });
  });

  function saveToSession() {
    sessionStorage.setItem(
      "contextFiles",
      JSON.stringify(
        documents.map((d) => ({
          path: d.path,
          _visible: d._visible,
          _checked: d._checked,
        })),
      ),
    );
  }

  async function modalClose() {
    openState = false;
    onChange(documents.filter((d) => d._checked).map((d) => d.path));
    saveToSession();
  }

  function onClose(path: string) {
    const i = documents.findIndex((d) => d.path === path);
    if (i === -1) return;
    documents[i]._visible = false;
    documents[i]._checked = false;
    documents = documents;
    onChange(documents.filter((d) => d._checked).map((d) => d.path));
    saveToSession();
  }

  function onToggle(path: string, b: boolean) {
    const i = documents.findIndex((d) => d.path === path);
    if (i === -1) return;
    documents[i]._checked = b;
    onChange(documents.filter((d) => d._checked).map((d) => d.path));
    saveToSession();
  }
</script>

<div class="flex space-x-1 w-full flex flex-wrap gap-0.5 mb-0">
  <button
    type="button"
    class="btn btn-sm flex items-center space-x-1 preset-filled-secondary-500 px-1 pr-2 h-5"
    disabled={loading}
    title="Explicitly insert files as context"
    onclick={() => (openState = true)}
  >
    <icons.Plus size={16} />Add context
  </button>
  {#each documents.filter((d) => d._visible) as doc, i (doc.path)}
    <div transition:fly={{ y: 10, duration: 150 }} class="relative2">
      <Checkbox
        name={doc._name}
        checked={doc._checked}
        title={doc.path}
        {loading}
        id={String(hash(doc.path))}
        onToggle={(b: boolean) => onToggle(doc.path, b)}
        onClose={() => onClose(doc.path)}
      />
    </div>
  {/each}
</div>

<Modal
  open={openState}
  onOpenChange={(e) => (openState = e.open)}
  triggerBase="btn preset-tonal"
  contentBase="card bg-surface-100-900/70 p-4 space-y-4 shadow-xl max-w-screen-sm"
  backdropClasses=""
>
  <!-- {#snippet trigger()}Open Modal{/snippet} -->
  {#snippet content()}
    <header class="flex justify-between">
      <div class="h6">Available Context Files</div>
    </header>
    <hr class="hr" />
    <article>
      {#if documents.length === 0}
        <p>No context files available.</p>
      {:else}
        <div class="whitespace-wrap text-sm mb-4">
          Selected documents can be explicitly included in the context.
        </div>

        <input
          type="text"
          class="input text-sm my-1"
          placeholder="Type to filter"
          bind:value={filterValue}
        />
        <div
          class="h-96 max-h-96 max-w-[80vw] overflow-auto scrollbar-hide text-sm
          p-4 border border-surface-200-800 rounded text-xs"
        >
          {#each filteredDocs as doc, i (doc.path)}
            <div
              class="hover:bg-surface-200-800 odd:bg-surface-100-900 px-2
              flex items-center space-x-2 border-b border-surface-200-800
              {doc._visible ? 'font-bold' : ''}"
              transition:fade
            >
              <input
                type="checkbox"
                class="checkbox w-4 h-4 p-0 m-0 accent-primary-500"
                id={`document-checkbox-${i}`}
                bind:checked={filteredDocs[i]._visible}
                onchange={(e: Event) => {
                  doc._checked = (
                    (e as InputEvent).target as HTMLInputElement
                  )?.checked;
                }}
                disabled={loading}
              />
              <label
                class="p-2"
                for={`document-checkbox-${i}`}
                title={bytesToSize(doc.size) +
                  ", Last modified " +
                  new Date(doc.lastModified * 1000).toLocaleString()}
              >
                {doc.path}
              </label>
            </div>
          {/each}
        </div>
      {/if}
      <div class="my-4 flex justify-end space-x-2 text-sm">
        <span>Selected documents:</span>
        <span class="font-medium">
          {documents.filter((d) => d._visible).length}/{documents.length}
        </span>
      </div>
    </article>
    <footer class="flex justify-end gap-4">
      <button type="button" class="btn preset-filled" onclick={modalClose}>
        Finish
      </button>
    </footer>
  {/snippet}
</Modal>
