# Superseded PCB work

`initial_THT_Base/` preserves the first Base conversion, before the multiplexer module and trimmer changes. The early state is also represented by the second commit in this repository.

`base_2layer_draft/` preserves the later two-layer Base attempt. It is **not fabrication-ready**: the last recorded DRC had six signal connections and one ground connection open. `base_2layer_trials/` contains intermediate routing experiments. Open these only as historical design files, not as the current Base.

`kicad_auto_history.tar.gz` is an archive of KiCad's local saved snapshots from the pre-reorganization workspace. It is kept as a recovery record; the root Git commits provide the curated project timeline.
