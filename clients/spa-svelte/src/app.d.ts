interface SettingsType {
  completionApis: ModelItem[];
  currentApi: string;
}

interface ChatParametersType {
  temperature: number;
  settings: SettingsType;
}

interface ModelItem {
  id: string;
  name: string;
  url: string;
  model: string;
  current: boolean;
  combinedPrice: number;
}

interface ChatMessage {
  role: "user" | "assistant" | "system";
  content: string;
  _html: string;
}