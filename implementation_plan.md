# Unreal Agentic Bridge — Piano di Implementazione

**Versione:** 6.0 — Post Analisi Ingegneristica  
**Data:** 2026-04-20  
**Repository:** [github.com/oprincipe/unreal-agentic-bridge](https://github.com/oprincipe/unreal-agentic-bridge)  
**Stato Attuale:** Infrastruttura bridge funzionante (Blueprint authoring, Behavior Tree manipulation, Blackboard management). Compilato e testato su UE 5.7.  
**Obiettivo:** Evolvere il sistema da "bridge MCP" a **Search-based Autonomous Game AI Compiler**.

---

## Contesto e Motivazione

### Stato dell'arte (mercato)
I bridge MCP per Unreal Engine sono una commodity emergente. Diversi progetti open-source espongono già centinaia di tool per manipolare l'editor via agenti LLM. La differenziazione non può basarsi sulla quantità di endpoint esposti.

### Il problema irrisolto nel settore
Tutti i sistemi esistenti sono **stateless e fire-and-forget**: l'LLM invia un comando, il bridge lo esegue, nessuno verifica se il risultato ha senso. Il nemico potrebbe rimanere incastrato in un muro, ignorare il giocatore, o muoversi in modo grottesco — il bridge dichiara comunque "successo" perché il nodo è stato creato senza errori di compilazione.

### La nostra tesi
L'unico modo per creare valore difendibile è spostare la validazione dal **livello sintattico** (il codice compila) al **livello semantico** (il comportamento funziona nel mondo simulato). Il prodotto diventa un compilatore di comportamento: riceve un intent in linguaggio naturale, esplora soluzioni, le valuta in simulazione, e converge sulla migliore.

---

## Architettura Target

```
┌──────────────────────────────────────────────────────────┐
│                    LLM Agent (Python)                     │
│  ┌─────────┐  ┌──────────────┐  ┌─────────────────────┐  │
│  │ Solution │  │ Convergence  │  │   State Memory      │  │
│  │ Tree     │  │ Policy       │  │   (Solution Library) │  │
│  └────┬─────┘  └──────┬───────┘  └──────────┬──────────┘  │
│       │               │                     │              │
│       └───────────────┼─────────────────────┘              │
│                       ▼                                     │
│              ┌────────────────┐                             │
│              │  IR Generator  │                             │
│              │  (JSON Schema) │                             │
│              └───────┬────────┘                             │
└──────────────────────┼──────────────────────────────────────┘
                       │ Validated IR (JSON)
                       ▼
┌──────────────────────────────────────────────────────────┐
│              Unreal Agentic Bridge (C++)                  │
│  ┌──────────┐  ┌──────────────┐  ┌────────────────────┐  │
│  │ IR       │  │ Deterministic│  │  Structured        │  │
│  │ Renderer │→ │ Simulation   │→ │  Report Generator  │  │
│  │ (atomic) │  │ Runner       │  │  (timeline+metrics)│  │
│  └──────────┘  └──────────────┘  └────────────────────┘  │
│                       ▲                                    │
│              Transaction Layer (Undo/Redo)                 │
└──────────────────────────────────────────────────────────┘
                       │
                       ▼
              ┌────────────────┐
              │  Scoring       │
              │  Engine        │
              │  (Python)      │
              │  multi-metric  │
              │  + constraints │
              └────────────────┘
                       │
                       ▼
              Score + Causal Diff → LLM per iterazione successiva
```

---

## Fasi di Sviluppo

### FASE 1 — Intermediate Representation (IR)
**Priorità:** P0 (bloccante per tutte le fasi successive)  
**Effort stimato:** 1–2 settimane  
**Prerequisiti:** Nessuno (si appoggia sull'infrastruttura C++ esistente)

**Obiettivo:** Disaccoppiare completamente l'LLM dall'engine. L'agente non manipola mai asset Unreal direttamente. Genera un documento JSON dichiarativo (la IR) che descrive il comportamento desiderato. Un renderer C++ nel plugin converte l'IR in asset Unreal in un singolo pass atomico transazionale (wrappato in `GEditor->BeginTransaction`).

**Deliverable:**
- Schema IR v1 per Behavior Tree + AIPerception + EQS, validato con Pydantic (Python)
- Renderer C++ (`HandleRenderIR`) che converte IR → asset Unreal con rollback automatico in caso di errore
- L'IR è versionato (`ir_version: "1.0.0"`). Quando Epic aggiorna un'API, si migra solo il renderer, non il sistema

**Esempio di IR:**
```json
{
  "ir_version": "1.0.0",
  "behavior": {
    "type": "patrol_with_detection",
    "patrol": {
      "strategy": "sequential",
      "points": ["PatrolPoint_A", "PatrolPoint_B", "PatrolPoint_C"],
      "wait_at_point_s": 2.0
    },
    "detection": {
      "senses": [
        {"type": "sight", "range": 1500, "fov_degrees": 90},
        {"type": "hearing", "range": 800}
      ],
      "on_detect": "chase_target"
    },
    "chase": {
      "acceptable_radius": 150,
      "timeout_s": 10,
      "on_lost_target": "return_to_patrol"
    }
  }
}
```

**Rischi identificati e mitigazioni:**
- *IR troppo semplice → inutile:* Lo schema copre i pattern comportamentali più comuni nella game AI (patrol, detect, chase, flee). I pattern non coperti rimangono editabili via tool atomici legacy.
- *IR troppo complesso → LLM non la genera:* Il formato è piatto e dichiarativo. Nessuna nesting profonda. L'LLM non deve conoscere l'API di Unreal.
- *Versioning UE:* L'IR è il contratto stabile. Il renderer è l'unico punto di accoppiamento con l'engine.

---

### FASE 2 — Deterministic Simulation Sandbox
**Priorità:** P0  
**Effort stimato:** 1–2 settimane  
**Prerequisiti:** FASE 1 (l'IR deve esistere per avere qualcosa da simulare)

**Obiettivo:** Permettere all'agente di eseguire il comportamento generato in una simulazione in-engine deterministica e ricevere un report strutturato (non binario) di quello che è accaduto.

**Deliverable:**
- Handler C++ `run_deterministic_simulation` che lancia PIE headless con `FApp::SetFixedDeltaTime`, seed deterministic, e timeout
- Report JSON strutturato con timeline di eventi, metriche calcolate, e stato finale
- Multi-seed support: la stessa IR viene testata con N seed diversi. Il report aggregato include media, deviazione standard e worst-case per ogni metrica (mitiga overfitting a scenario statico)
- Progressive simulation: primo test a 5s (fast-fail), se superato scala a 15s, poi a 30s (riduce costo computazionale)

**Report di esempio:**
```json
{
  "seeds_tested": [42, 137, 256],
  "aggregate": {
    "patrol_completion": {"mean": 0.78, "std": 0.11, "worst": 0.66},
    "path_efficiency": {"mean": 0.82, "std": 0.05, "worst": 0.75},
    "stuck_events": {"mean": 0.0, "std": 0.0, "worst": 0},
    "idle_ratio": {"mean": 0.12, "std": 0.03, "worst": 0.18}
  },
  "per_seed_timeline": { ... }
}
```

**Rischi identificati e mitigazioni:**
- *Test flaky (physics jitter):* Fixed timestep + seed deterministico. Stesso input = stesso output. Se due run divergono → errore interno, non inoltrato all'LLM.
- *Costo computazionale:* Headless mode (`-nullrhi`), progressive simulation (early termination se fallisce nei primi 5s), simulation caching (hash dell'IR → se già testata, riusa il report).
- *Domain randomization:* Multi-seed varia le condizioni iniziali. Impedisce soluzioni che funzionano solo nello scenario ideale.

---

### FASE 3 — Multi-Objective Scoring & Search
**Priorità:** P1  
**Effort stimato:** 1–2 settimane  
**Prerequisiti:** FASE 2 (serve il report strutturato per calcolare lo score)

**Obiettivo:** Sostituire la logica "retry lineare finché passa" con un sistema di ricerca nello spazio delle soluzioni che classifica, confronta, e ramifica dai tentativi migliori.

**Deliverable:**

**3.1 — Objective Function multi-metrica (Python)**
```python
objective = ObjectiveFunction(
    hard_constraints=[
        "patrol_completion.worst >= 0.66",
        "stuck_events.worst == 0",
    ],
    soft_metrics={
        "patrol_completion.mean": {"weight": 0.35, "target": 1.0},
        "path_efficiency.mean":   {"weight": 0.25, "target": 0.85},
        "idle_ratio.mean":        {"weight": 0.15, "target_max": 0.1},
        "detection_accuracy.mean":{"weight": 0.15, "target": 1.0},
        "behavior_smoothness":    {"weight": 0.10, "target_max": 5},
    }
)
```
- Hard constraints filtrano preventivamente (la soluzione viene scartata indipendentemente dallo score)
- Soft metrics calcolano uno score composito `[0.0 – 1.0]`
- Il punteggio è calcolato dal sistema, non dall'LLM

**3.2 — Solution Tree con branching**
```
Root (empty BT)
├── Attempt 1: Sequence→MoveTo         score=0.20  ✗ patrol_fail
│   ├── Attempt 2: +EQS Generator      score=0.55  ✗ detection_fail
│   │   ├── Attempt 4: +Sight Sense    score=0.72  ★ best
│   │   └── Attempt 5: +Hearing Sense  score=0.45  ✗ regression
│   └── Attempt 3: +Wait node          score=0.30  ✗ idle_high
```
- L'LLM riceve ranking comparativo, non solo l'ultimo errore
- Può ramificare dal tentativo migliore (non solo dall'ultimo)
- Causal Diff Analysis automatica: il sistema calcola quali metriche sono migliorate/peggiorate e perché

**3.3 — Convergence Policy**
```python
search_policy = SearchPolicy(
    max_iterations=8,
    max_wall_time_minutes=15,
    min_acceptable_score=0.75,
    early_stop_if_no_improvement_for=3,
    on_budget_exhausted="return_best",
)
```
- Budget computazionale esplicito e prevedibile
- Early stop se il sistema non converge
- Sempre restituisce la migliore soluzione trovata, anche se sotto target

**Rischi identificati e mitigazioni:**
- *Pesi arbitrari nella reward function:* I pesi sono configurabili dall'utente e dai template di progetto. In futuro, Pareto front ranking può sostituire la weighted sum per eliminare la dipendenza dai pesi.
- *Local minima (search exploitativo):* L'LLM viene istruito tramite prompt a tentare perturbazioni random ogni N tentativi (random restart strategy). In futuro, beam search con k soluzioni parallele.
- *Costi da loop:* Budget rigido (max 8 iterazioni, max 15 minuti). Progressive simulation riduce il costo per iterazione.

---

### FASE 4 — Solution Library & Learning (Long-term)
**Priorità:** P2  
**Effort stimato:** 2–4 settimane (in corso durante le fasi successive)  
**Prerequisiti:** FASE 3 funzionante

**Obiettivo:** Il sistema non deve ripartire da zero ogni volta. Le soluzioni validate vengono archiviate e riutilizzate come punto di partenza per problemi simili. Qui nasce il moat basato sui dati.

**Deliverable:**
- Solution Library persistente: ogni soluzione che supera `min_acceptable_score` viene archiviata con la sua IR, lo score, e il report di simulazione
- Retrieval per similarità: quando l'utente chiede un nuovo comportamento, il sistema cerca nella library la soluzione più simile e la usa come punto di partenza (riducendo le iterazioni necessarie)
- Pattern extraction: dopo N soluzioni accumulate, il sistema identifica pattern ricorrenti (es: "ogni Patrol con Detection ha bisogno di un Wait dopo il Sense trigger") e li inietta nei prompt futuri
- Behavior heuristics: metriche di qualità semantica (jerkiness, direction entropy, reaction time) calcolate automaticamente dal Simulation Report

---

## Proof of Concept

**Scenario di validazione:** Un agente LLM riceve l'obiettivo: *"Crea un nemico che pattuglia 3 waypoint in sequenza e insegue il giocatore se entra nel suo campo visivo."*

**Criteri di successo:**
1. L'agente genera una IR valida senza conoscere l'API di Unreal
2. Il renderer C++ converte l'IR in un Behavior Tree + AIPerception funzionante
3. La simulazione deterministica produce un report strutturato
4. Il sistema converge a una soluzione con score ≥ 0.75 in ≤ 8 iterazioni
5. La soluzione è riproducibile (stesso seed = stesso risultato)

**Criteri di non-successo (cosa il sistema NON fa e NON promette):**
- Non sostituisce il game design umano
- Non genera livelli o narrative
- Non opera su problemi non strutturabili
- Non garantisce ottimalità globale — converge su "buono abbastanza"

---

## Timeline Complessiva

| Fase | Settimane | Deliverable Chiave | Gate di Approvazione |
|------|-----------|--------------------|--------------------|
| FASE 1 (IR) | 1–2 | Schema Pydantic + Renderer C++ | IR genera un BT funzionante in Unreal |
| FASE 2 (Simulation) | 1–2 | Simulation Runner + Structured Report | Report deterministico riproducibile |
| FASE 3 (Search) | 1–2 | Scoring + Solution Tree + Convergence | PoC supera criteri di successo |
| FASE 4 (Learning) | Ongoing | Solution Library + Retrieval | Riduzione iterazioni su problemi simili |

**Tempo totale stimato per PoC (Fasi 1–3):** 3–6 settimane.

---

## Posizionamento Competitivo

| Caratteristica | Bridge MCP Standard | Unreal Agentic Bridge (dopo Fasi 1–3) |
|---------------|--------------------|-----------------------------------------|
| Manipolazione asset | ✅ | ✅ |
| Validazione sintattica | ✅ (compila/non compila) | ✅ |
| Validazione semantica | ❌ | ✅ (simulazione deterministica) |
| Rollback | ❌ | ✅ (transaction layer) |
| Multi-objective scoring | ❌ | ✅ (funzione obiettivo configurabile) |
| Search nello spazio soluzioni | ❌ (retry lineare) | ✅ (solution tree + convergence) |
| Intermediate Representation | ❌ (accoppiato all'engine) | ✅ (IR versionata e stabile) |
| Solution library | ❌ | ✅ (Phase 4, il moat a lungo termine) |

---

## Limiti Noti

- **Costo computazionale:** Ogni iterazione richiede render IR + simulazione + scoring. Mitigato da progressive simulation e caching, ma il sistema è computazionalmente più pesante di un bridge stateless.
- **Dipendenza dalla qualità dell'LLM:** Il sistema guida l'LLM tramite feedback strutturato, ma non può compensare un modello fondamentalmente incapace di ragionamento causale.
- **Copertura limitata ai pattern strutturati:** Patrol, detect, chase, flee sono ben coperti. Comportamenti emergenti e creativi richiedono intervento umano.
- **Non è un prodotto finito:** È un'infrastruttura di ricerca. Il valore cresce con l'accumulo di soluzioni nella library (moat basato su dati, non su codice).
