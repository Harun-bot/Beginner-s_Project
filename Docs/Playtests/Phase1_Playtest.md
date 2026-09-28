# Phase 1 playtest script: traversal

Section 21: playtests every 2 weeks with a 1 to 10 feel survey; **observe, don't explain**.
The Phase 1 Definition of Done needs **5 testers** and an average feel score of **8 or higher**.

## Before each tester

- Development build or Play in Editor on `L_PerfRoute_Swing`. Start with a fresh `DA_TraversalTuning`, and write
  down which version of it you're testing.
- Stats HUD on (F1); tuning panel off.
- Note the tester's experience (plays action games? played Spider-Man games?) and input device.

## What to say (only this)

> "You're a web-slinging hero in a city. Hold Shift in the air to swing, Space to jump or zip. I can't help while you
> play: just say out loud what you're thinking."

Then say nothing. Don't explain controls, the release window, wall-running or the park, and don't answer "how do I…"
questions. Write the question down; it's data.

## Session (about 15 minutes)

| Time | Tester does | You write down |
|---|---|---|
| 0–1 min | First swing | Seconds until the first swing (target: under 60 s, Section 2). First reaction, word for word. |
| 1–6 min | Free practice | Where they touch down and why (street? roof? wall?). Moments of confusion. |
| 6–9 min | "Swing as far as you can without touching the ground." | Best "Airborne" distance from the HUD (the DoD target is 2,000 m). |
| 9–11 min | "Get to the top of the tallest tower." | Did they find the wall-run? The zip? How long did it take? |
| 11–13 min | "Cross the park." | Did they get stuck? Did they find the sky anchors, tree webs or glide? |
| 13–15 min | "Try to get stuck anywhere." | Any place where nothing works: a **dead end** is a DoD failure. Screenshot it and note where it was (HUD height and the spot on the map). |

Afterwards, copy `Saved/Traversal/Runs.csv` and note the tester's row range.

## Survey (fill in `Phase1_Survey.csv`, one row per tester)

Ask each question, give no hints, and record the number and one sentence of "why".

1. **Felt like a web-slinger** (1–10): "How much did swinging feel like being a web-slinging superhero?" *(the DoD score)*
2. **Control** (1–10): "How much did the hero do what you wanted?"
3. **Speed** (1–10): "How fast did it feel?" (1 = sluggish, 10 = thrilling)
4. **Readability** (1–10): "How clear was where your web would stick?"
5. **Frustration** (1–10): "How often did it do something you didn't expect?" (1 = never, 10 = constantly)
6. "What was the best moment?"
7. "What was the most annoying moment?"
8. "If you could change one thing, what would it be?"

## Scoring

- **Pass:** average of question 1 over 5 testers ≥ 8, no dead ends, at least one tester reaching 2,000 m airborne.
- If the average is below 8: look at questions 2–5 and the touch-down notes, change **one** group of tuning values
  (see Docs/TraversalTuning.md), and test again with new testers where possible.
