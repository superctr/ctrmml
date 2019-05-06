MML reference for ctrmml (2019-05-06)
========================

## Meta commands
Lines starting with # or @ define a meta command. `#` parameters are always
assumed to be a string, whereas `@` commands define a table of space- or
comma-separated values. Strings can be enclosed in double quotes if needed.

-	`#title`, `#composer`, `#author`, `#date`, `#comment` - Song metadata.
-	`#platform` - Sets the MML target platform.
	- **TODO**: available platforms
-	`@<num>` - Defines an instrument. Parameters are platform-specific.
-	`@E<num>` - Defines an envelope.
-	`@M<num>` - Defines a pitch envelope.
-	`@P<num>` - Defines a pan envelope.

## Addressing tracks
A span of characters at the beginning of a line selects tracks. There must be
no space between the beginning of the line and the first character in the
track list.

You can specify channels in any order, this affects the `{/}` conditional
block.

### Macros
`*<num>` can be used to specify a track by its number. Since tracks 32 and
above are unlikely to be used by sound channels, you can use them as macros
with the `*` command.

This is not a substitution macro but rather more like a subroutine.
Each track has its own octave and length counter.

### Example:
-	`A` - use track A
-	`ABC` - use tracks A, B, C
-	`ACB` - use tracks in a different order
-	`*0` - use track number 0.

## Adding notes and commands
After specifying channels and adding spaces, you can enter MML commands. It is
also possible to write a new line without specifying tracks, in that case
the previously specified channels will be used.

### Example:
	A       cdefgab
	        bagfedc ; both use track A

## Command reference
-	`cdefgabh` - Notes. Optionally set duration after each note. If no
	duration is set, the duration set by the `l` command is used.
	- Durations are calculated by dividing the value with the length of a
	measure. Optionally a dot `.` can be specified to extend the duration by
	half. Specify duration in number of frames with the `:` prefix.
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
-	`D<0..255>` - Drum mode. If set to a non-zero value, enable drum mode and
	set the drum mode index to the parameter value.
	- This changes the behavior of notes. `abcdefgh` represent 0-7 and is
	added to the drum mode index. The macro track with this number is then
	executed up to and including the first note encountered.
	- The duration specified in the macro track is ignored and the calling
	track is used instead.
-	`*<0..255>` - Play commands from another track before resuming at the
	current position.
-	`R<duration>` - Reverse rest. This subtracts the value from the previous
	note or rest.
	- This can be used to bring back tracks in sync after a delayed echo
	track.
	- If unable (such as if the previous note was at the end of a
	loop, a warning is issued.
-	`~<note><duration>` - Grace note. This subtracts from the length of the
	previous note like the `R` command, then adds a note with the duration of
	the borrow.
-	`t<0..255>` - Set tempo in BPM.
-	`T<0..255>` - Set tempo using the platform's native timer values. &ast;
-	`[/]<0..255>` - Loop block. The section after the `/` is skipped at the
	last iteration.
-	`L` - Set loop point (segno). If this is present, playback resumes at this
	point when the end of the track is reached.
-	`{/}` - Conditional block. Using this block you can specify commands to
	run on each	individual track, when multiple tracks are specified. Example:
	`ABC {c/d+/g} {d/f/a}`
-	`;` - Comment. The rest of the line is skipped.
-	`|` - Divider. This has no effect and can be used to divide measures or
	bars, for aesthetic reasons.

&ast; indicates that the range may depend on the platform.

### Comparison with other MML formats (incomplete)
#### PMD
-	`l-<duration>` (borrow), use the `R<duration>` command instead.
-	`&` (tie), use the `^` command.
-	`&&` (slur), use the `&` command.

## Platforms
### Megadrive
#### Channel mapping
-	`ABCDEF` FM channels 1-6.
-	`GHI` PSG melody channels 1-3.
-	`J` PSG noise channel.

#### Limitations
Pan envelopes not supported.

Panning using the `p` command is only allowed for FM channels and the accepted
range is 0-3. Bit 1 enables the right channel, bit 2 enables the left channel.

#### FM instruments
FM instruments are defined as below: (Commas between values are optional)

	@1 fm ; finger bass
	;	ALG  FB
		  3   0
	;	 AR  DR  SR  RR  SL  TL  KS  ML  DT SSG
		 31   0  19   5   0  23   0   0   0   0
		 31   6   0   4   3  19   0   0   0   0
		 31  15   0   5   4  38   0   4   0   0
		 31  27   0  11   1   0   0   1   0   0

##### 2op chord macro
Instrument type `2op` is used to duplicate FM instruments, modifying the
operators' multiply ratios and setting a transpose (useful if the base note
uses a multiplier which is not a power of 2). Disabled carrier modulators are
also enabled, which can be useful if you use the same patch for both
monophonic and polyphonic sounds.

	@2 fm ;2op lead
	;	ALG  FB
		  4   0
	;	 AR  DR  SR  RR  SL  TL  KS  ML  DT SSG
		 20   5   0   1   1   9   0   4   7   0
		 31   8   4   7   2   0   0   4   0   0
		 20   5   0   1   1   9   0   1   7   0
		 31   8   4   7   2 127   0   1   0   0
	;         @ ML1 ML2 ML3 ML4 TRS
	@24 2op   2   5   5   4   4   0 ; n+4
	@25 2op   2   4   4   3   3   5 ; n+5
	@26 2op   2   7   7   5   5  -4 ; n+6
	@27 2op   2   6   6   4   4   0 ; n+7
	@28 2op   2   8   8   5   5  -4 ; n+8

#### PSG instruments
PSG instruments (envelopes) are defined as a sequence of values. 15 is max
volume and 0 is silence.

	@10 psg
		1 4 6 8 10 12 13 14 15

Use `>` to specify sliding from one value to another (important: no space)

	@11 psg
		15>10

Use `:` set the length of each value (in frames).

	@12 psg
		15:10     ; vol 15 for 10 frames
		15>0:100  ; from 15 to 0 in 100 frames

Set the sustain position with `/` or the loop position with `|`

	@13 psg
		15 14 / 13>0:7
	@14 psg
		0>14:7 | 15 10 5 0 5 10

When specifying envelopes, there must be no space between the
parameters in a node, as the space itself separates nodes.

#### Pitch envelopes
Pitch envelopes are specified with "@M<number>" and are used in MML
sequences with the M<number> command.

Pitch envelopes consist of up to 256 envelope nodes. Each node has
an initial position, a length parameter and a delta parameter.
The delta parameter is calculated from a "target" parameter and the
length parameter.

The parameters in the node are specified like this. As with PSG
envelopes, there must be no space between parameters in one envelope
node.

##### Commands
The initial and target parameters are defined in semitones. They CAN be
fractional.

-	`value>target:length` - Inserts a node with initial value,
	slide target and length.
-	`value>target` - Inserts a node with initial value and slide
	target. The length depends on the depth of the slide. **TODO**: describe
-	`value:length` - Inserts a node with length.
-	`value` - Inserts a node with a single frame. This can be placed at
	the end of the envelope to stop an infinite slide.
-	`|` Set loop position
-	`Vvalue:depth:rate` - Vibrato macro.
	- `value` is the pitch bias,
	- `depth` sets the depth of the vibrato in semitones.
	- `Rate` defines the duration of 1/4 of the vibrato waveform.
	Lower value = faster.

##### Examples
Simple arpeggio:

	@M1 | 0 3 5

Pitch slide from 2 semitones over 20 frames

	@M1 -2>0:20 0 ;last '0' position needed to stop the slide

Vibrato with 20 frame delay at the beginning

	@M1 0:20 | 0>0.5:5 0.5>-0.5:10 -0.5>0:5

Using the `V` macro to do the same thing

	@M1 0:20 | V0:1:5
