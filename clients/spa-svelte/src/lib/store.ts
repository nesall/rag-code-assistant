import { writable } from 'svelte/store';

export const temperature = writable<number>(0.1);
export const settings = writable<SettingsType>({ completionApis: [], currentApi: "" });
export const messages = writable<ChatMessage[]>([]);