# Assets and issues

The Assets dock lists the images, fonts, sounds and scenes of the project and of the engine: drag an image onto the view for a new Sprite, onto a Sprite to change its texture, or a font for a new Label; a double click drops it in the middle.

The Issues tab checks the project's assets against the platform being previewed: textures over 1024 pixels on the 3DS and DS, DS audio that is not mono PCM WAV, audio on the fx-CG50, a project that does not fit its 2 MB, and an image in `sce_sys/` that is not a palette PNG, which the Vita's installer refuses. An issue with a fix has a button, and the file as it was is kept in `.fried-editor/originals/` in the project.
