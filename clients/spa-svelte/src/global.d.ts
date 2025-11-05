declare interface Window {
  apiServerUrl: string | undefined;
  cppApi: {
    setServerUrl: (url: string | undefined) => any;
    getServerUrl: () => Promise<string | undefined>;
    setPersistentKey: (key: string, value: string) => Promise<void>;
    getPersistentKey: (key: string) => Promise<string | null>;
  };
  hljs: {
    highlightAll: () => any;
  }
  HLJS_CUSTOM: {
    initHljs: () => any;
  }
}