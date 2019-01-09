MML reference
=============

### Addressing tracks
A span of characters at the beginning of a line selects tracks. You can
specify channels in any order (important later).

#### Example:
-	`A` - use track A
-	`ABC` - use tracks A, B, C
-	`ACB` - use tracks in a different order

### Adding notes and commands
After specifying channels and adding spaces, you can enter MML commands. It is
also possible to write a new line without specifying tracks, in that case
the previously specified channels will be used.

#### Example:
	A       cdefgab
			bagfedc ; both use track A

### Command reference
-	`cdefgab` - Notes. Optionally set duration after each note.
	- Durations are calculated by dividing the value with the length of a
	measure. Optionally a dot `.` can be specified to extend the duration by
	half.
-	`^` - Tie. Extends duration of previous note.
-	`&` - Slur. Used to connect two notes (legato).
-	`r` - Rest. Optionally set duration after the rest.
-	`l<1..256>` - Set default duration, used if not explicitly specified after
	a note.
-	`q<1..8>` - Quantize. Used to set articulation.
-	`>`, `<` - Octave up and down, respectively.
-	`o<0..7>` - Set octave.
-	`k<-127..127>` - Set transpose. (*)
-	`K<-127..127>` - Set detune. (*)
-	`v<0-15>` - Set volume. (*)
-	`)`, `(` - Volume up and down, respectively. You may specify the increment
	or decrement, if you want.
-	`V<0-255>` - Set volume (fine). (*)
-	`@<0-65535` - Set instrument. (*)
-	`t<0-255>` - Set tempo in BPM.
-	`T<0-255>` - Set tempo using the platform's native timer values. (*)
-	`[/]<0-255>` - Loop block. The section after the `/` is skipped at the
	last iteration.
-	`L` - Set loop point (segno). Playback is resumed at this point.
-	`{/}` - Conditional block. Using this block you can specify commands to
	run on each	individual track, when multiple tracks are specified. Example
	below:
		ABC {c/d+/g} {d/f/a}
-	`;` - Comment. The rest of the line is skipped.
-	`|` - Divider. This has no effect and can be used to divide measures or
	bars, for aesthetics.

(*) indicates that the range may depend on the platform.
