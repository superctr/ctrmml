#!/usr/bin/env python3
from math import log10
from argparse import ArgumentParser

from mido import MidiFile, tempo2bpm, merge_tracks

"""
MIDI to ctrmml converter.

Suggested usage is to create your own template MML containing the
instrument definitions, then append the output from this script to the template.

Note that when the script is run as-is, the conversion is very basic.

You could import this module in your own scripts and provide callbacks to mid2mml()
for a more detailed conversion.
"""

class Timeline(dict):
    def __missing__(self, key):
        return ""
class MMLTracks(dict):
    def __missing__(self, key):
        return Timeline()

# MIDI to MML channel map.
# This default converts MIDI channel 0 to MML track A, channel 1 to B, and so on.
MML_CHANNEL_MAP = 'ABCDEFGHIJ'

def note2mml(midi_note):
    """
    Convert MIDI note number to MML octave+note string
    """
    notes = ['c', 'c+', 'd', 'd+', 'e', 'f', 'f+', 'g', 'g+', 'a', 'a+', 'b']
    return f'o{int(midi_note / 12) - 1}{notes[int(midi_note % 12)]}'

def vol2mdsdrv(vol):
    """
    Convert MIDI volume to FM total level
    """
    vol = ((-40 * log10(vol/127)) / 0.75) + 0.5
    if vol > 127:
        vol = 127
    return int(vol)

def pan2mdsdrv(pan):
    """
    Convert MIDI panning to FM enable flags
    """
    if msg.value < 64:
        return 2
    elif msg.value == 64:
        return 3
    else:
        return 1

def note(msg: mido.Message, state=None) -> str:
    """
    Default note callback.
    Note that this is called on note-off as well, in order
    to compare the note string with the previous value.
    """
    return note2mml(msg.note)

def program_change(msg: mido.Message, state=None) -> str:
    """
    Default program change callback.
    Return a string or None here.
    """
    return f'@{msg.program}'

def control_change(msg: mido.Message, state=None) -> str:
    """
    Default control change callback
    Return a string or None here.
    """
    if msg.is_cc(7): # channel volume
        return f"V{vol2mdsdrv(msg.value)}"
    elif msg.is_cc(10): # panning
        return f"p{pan2mdsdrv(msg.value)}"
    else:
        return None

messages_with_channel = ['note_on', 'note_off', 'polytouch', 'control_change',
                         'program_change', 'aftertouch', 'pitchwheel']

def mid2mml(mid: mido.MidiFile,
            channel_map=MML_CHANNEL_MAP,
            state=None,
            on_note=note,
            on_control_change=control_change,
            on_program_change=program_change) -> str:
    """
    Converts a MIDI file to a string containing MML data.

    Limitations:
      - Type 1 files only.
      - No polyphony.
      - Only program change, volume, pan and note on/off is converted.
      - Note velocity and expression is ignored.
      - PPQN is always converted to 24. For best result, use this in your MIDI!

    However: You can provide your own callbacks for note, control change and
             program change messages.
    """
    if mid.type != 1:
        return "; Not a MIDI type 1 file!"

    ticks_divider = mid.ticks_per_beat / 24
    mml_tracks = MMLTracks()

    merged_tracks = {}
    for track in mid.tracks:
        mml_track_id = channel_map[0]
        for msg in track:
            if msg.type in messages_with_channel:
                mml_track_id = channel_map[msg.channel]
                break
        if mml_track_id in merged_tracks:
            merged_tracks[mml_track_id] = merge_tracks([merged_tracks[mml_track_id], track])
        else:
            merged_tracks[mml_track_id] = track

    for mml_track_id, track in merged_tracks.items():
        timeline = mml_tracks[mml_track_id]
        time = 0
        note = 'r'
        prev_note = ''
        for msg in track:
            mml_time = int(msg.time / ticks_divider)
            if mml_time > 0:
                timeline[time] += note
                prev_note = note
                if note != 'r':
                    note = '^'
            time += mml_time
            if msg.type == 'set_tempo':
                tempo = int(tempo2bpm(msg.tempo))
                timeline[time] += str(f"t{tempo}")
            elif msg.type == 'program_change':
                s = on_program_change(msg, state)
                if s is not None:
                    timeline[time] += s
            elif msg.type == 'control_change':
                s = on_control_change(msg, state)
                if s is not None:
                    timeline[time] += s
            elif msg.type == 'note_on':
                note = on_note(msg, state)
            elif msg.type == 'note_off':
                if on_note(msg, state) == prev_note:
                    note = 'r'
        timeline[time] += ''
        mml_tracks[mml_track_id] = timeline

    output = ""
    for mml_track_id, timeline in mml_tracks.items():
        if not mml_track_id.isalpha():
            continue
        last_time = 0
        for time, mml in timeline.items():
            if (last_time != time):
                output += f':{time-last_time} ;{last_time}\n'
            output += mml_track_id + ' ' + mml
            last_time = time
        output += f';{last_time}\n\n'
    return output

if __name__ == '__main__':
    parser = ArgumentParser(prog='mid2ctrmml',
                            description='Converts MIDI files to MML. Outputs to stdout.')
    parser.add_argument('filename')
    parser.add_argument('-m', '--map',
                        type=str, default=MML_CHANNEL_MAP,
                        help='MIDI to MML to channel map')
    args = parser.parse_args()
    mid = MidiFile(args.filename)
    print(mid2mml(mid, channel_map=args.map))
