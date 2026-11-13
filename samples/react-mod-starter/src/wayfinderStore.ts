import { create } from "zustand";

export interface WayfinderStore {
  mapLocation: string;
  trackedQuestId: string;
  questProgress: Record<string, number[]>;
  setMapLocation: (location: string) => void;
  setTrackedQuestId: (questId: string) => void;
  toggleObjective: (questId: string, objectiveIndex: number) => {
    wasComplete: boolean;
    completedObjectives: number[];
  };
}

/** Shared preview state for the Skyrim map and its tracked field contracts. */
export const useWayfinderStore = create<WayfinderStore>((set, get) => ({
  mapLocation: "Whiterun",
  trackedQuestId: "ashbound",
  questProgress: { ashbound: [0], starfall: [] },
  setMapLocation: (mapLocation) => set({ mapLocation }),
  setTrackedQuestId: (trackedQuestId) => set({ trackedQuestId }),
  toggleObjective: (questId, objectiveIndex) => {
    const current = get().questProgress[questId] ?? [];
    const wasComplete = current.includes(objectiveIndex);
    const completedObjectives = wasComplete
      ? current.filter((index) => index !== objectiveIndex)
      : [...current, objectiveIndex];
    set((state) => ({
      questProgress: { ...state.questProgress, [questId]: completedObjectives },
    }));
    return { wasComplete, completedObjectives };
  },
}));
