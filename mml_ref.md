MML reference
=============

### Meta commands
Lines starting with # or @ defines a meta command. `#` parameters are always
assumed to be a string, whereas `@` commands define a table of comma-separated
values. Strings can be enclosed in double quotes if needed.

-	`#title`, `#composer`, `#author`, `#date`, `#comment' - Song metadata.
-	`#platform` - Sets the MML target platform.
	- todo: available platforms
-	`@<num>` - Defines an instrument. Parameters are platform-specific.
-	`@E<num>` - Defines an envelope.
-	`@M<num>` - Defines a pitch envelope.
-	`@P<num>` - Defines a pan envelope.

### Addressing tracks
A span of characters at the beginning of a line selects tracks. You can
specify channels in any order (important later).

#### Macros
`*<num>` can be used to specify a track by its number. Since tracks 32 and
above are unlikely to be used by sound channels, you can use them as macros
with the `*` command.

#### Example:
-	`A` - use track A
-	`ABC` - use tracks A, B, C
-	`ACB` - use tracks in a different order
-	`*0` - use track number 0.

### Adding notes and commands
After specifying channels and adding spaces, you can enter MML commands. It is
also possible to write a new line without specifying tracks, in that case
the previously specified channels will be used.

#### Example:
	A       cdefgab
			bagfedc ; both use track A

### Command reference
-	`cdefgabh` - Notes. Optionally set duration after each note. If no
	duration is set, the duration set by the `l` command is used.
	- Durations are calculated by dividing the value with the length of a
	measure. Optionally a dot `.` can be specified to extend the duration by
	half.
	- 'h' has the same value as 'b' in normal mode.
-	`^` - Tie. Extends duration of previous note.
-	`&` - Slur. Used to connect two notes (legato).
-	`r` - Rest. Optionally set duration after the rest.
-	`l<duration>` - Set default duration, used if not specified by notes,
	rests, `R` or `G` commands.
-	`q<1..8>` - Quantize. Used to set articulation.
-	`>`, `<` - Octave up and down, respectively.
-	`o<0..7>` - Set octave.
-	`k<-128..127>` - Set transpose. &ast;
-	`K<-128..127>` - Set detune. &ast;
-	`v<0-15>` - Set volume. &ast;
-	`)`, `(` - Volume up and down, respectively. You may specify the increment
	or decrement, if you want.
-	`V<0..255>` - Set volume (fine). &ast;
-	`p<-128..127>` - Set panning. &ast;
-	`@<0..65535>` - Set instrument. &ast;
-	`E<0..255>` - Set envelope. 0 to disable. &ast;
-	`M<0..255>` - Set pitch envelope. 0 to disable. &ast;
-	`P<0..255>` - Set pan envelope. 0 to disable. &ast;
-	`G<0..255>` - Set portamento. 0 to disable. &ast;
-	`D<0..255>` - If D is set to a non-zero value, enable drum mode and set the
	index to the parameter value. Notes `abcdefgh` represent 0-7 and is added
	to the index and commands are read from that macro track before playing a
	note c with duration set as usual.
-	`*<0..255` - Play commands from another track before resuming at the
	current position.
-	`R<duration>` - Reverse rest. This subtracts the value from the previous note
	or rest. If unable (such as if the previous note was at the end of a loop,
	a warning is thrown. This can be used to bring back tracks in unison after
	a delayed echo track.
-	`~<note><duration>` - Grace note. Basically similar to above, except a
	note is added with the same duration as the borrow.
-	`S<duration>,<value>` - Shuffle. When enabled, vary the length of each
	`<duration>` by `value` amount. Turn off with `S0`.
-	`t<0..255>` - Set tempo in BPM.
-	`T<0..255>` - Set tempo using the platform's native timer values. &ast;
-	`[/]<0..255>` - Loop block. The section after the `/` is skipped at the
	last iteration.
-	`L` - Set loop point (segno). Playback is resumed at this point.
-	`{/}` - Conditional block. Using this block you can specify commands to
	run on each	individual track, when multiple tracks are specified. Example:
	`ABC {c/d+/g} {d/f/a}`
-	`;` - Comment. The rest of the line is skipped.
-	`|` - Divider. This has no effect and can be used to divide measures or
	bars, for aesthetics.

&ast; indicates that the range may depend on the platform.

### Platforms
#### Megadrive
#### FM instruments
FM instruments are defined as below: (Commas between values are optional)

	@1	fm ; finger bass
	;	ALG  FB
		  3   0
	;	 AR  DR  SR  RR  SL  TL  KS  ML  DT SSG
		 31   0  19   5   0  23   0   0   0   0
		 31   6   0   4   3  19   0   0   0   0
		 31  15   0   5   4  38   0   4   0   0
		 31  27   0  11   1   0   0   1   0   0

#### PSG instruments
PSG instruments (envelopes) are defined as a sequence of values. 15 is max
volume and 0 is silence.

	@10	psg
		1 4 6 8 10 12 13 14 15

Use `>` to specify sliding from one value to another

	@11	psg
		15>10

Use `:` set the length of each value (in frames).

	@12	psg
		15:10     ; vol 15 for 10 frames
		15>0:100  ; from 15 to 0 in 100 frames

Set the sustain position with `/` or the loop position with `|`

	@13 psg
		15 14 / 13>0:7
	@14 psg
		0>14:7 | 15 10 5 0 5 10

