# QPitch architecture

These PlantUML diagrams describe the JUCE / Rubber Band implementation in QPitch 2.0.

- [Architecture](architecture.svg) ([source](architecture.puml)): host formats, processor, editor and DSP dependencies.
- [Classes](classes.svg) ([source](classes.puml)): principal classes and ownership.
- [Audio flow](audio-flow.svg) ([source](audio-flow.puml)): pitch control, linked stereo shifting, delayed dry path and measured output.
- [State and UI](state-and-ui.svg) ([source](state-and-ui.puml)): automation, session persistence, custom notes and live piano updates.

Render the diagrams from the repository root with PlantUML and Graphviz installed:

```sh
uml/render.sh
```

`style.puml` provides the shared appearance. The render script adds a solid white background to each SVG so it remains white in image viewers and on GitHub. SVG previews are included for viewing on GitHub. The diagrams describe logical interactions; they do not promise thread synchronization beyond the atomic pitch readouts implemented in the processor.
