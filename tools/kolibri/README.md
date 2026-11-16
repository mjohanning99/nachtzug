# German texts with Kolibri

Scripts that rework the program's German (`i18n/nachtzug_de.ts`) with a German language model running locally in
llama-server (the router on `127.0.0.1:8080`, model `kolibri`). The German program is impersonal: never "du" or
"Sie", infinitives on keys and in hints, the passive or "man" in messages, as in the manual.

1. `collect.py WelcomeDialog,SettingsDialog > batch.json`: the strings of those parts of the program, each with its
   English, the current German, where it sits (the code around it, the page or panel) and what kind of text it is.
2. `translate.py batch.json out.json`: sends them in batches of twelve, with a description of the program, the style
   rules and the fixed names; checks placeholders, markup and "du"/"Sie" in what comes back. Can be stopped and
   started again: it carries on where it was.
3. `review.py batch.json out.json page.html [notes.json]`: a page to compare English, the current German and Kolibri's.
4. `apply.py batch.json out.json fixes.json`: writes Kolibri's German into the translation file, or the fix where
   there is one (`null` keeps the current text).

Every result is read before it goes in: in the pilot Kolibri sometimes changed the meaning, dropped half a message,
left a word in English, broke the singular/plural forms or strayed from the program's names ("Sound-Tab").
