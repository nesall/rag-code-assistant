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

