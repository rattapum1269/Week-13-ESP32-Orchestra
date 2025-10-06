#!/usr/bin/env python3
"""
MIDI to Orchestra Converter
แปลงไฟล์ MIDI เป็นรูปแบบ Orchestra สำหรับ ESP32

การใช้งาน:
    python midi_to_orchestra.py input.mid output.h

หรือ:
    python midi_to_orchestra.py --help
"""

import sys
import argparse
import struct
from pathlib import Path

# MIDI Note mappings
NOTE_NAMES = {
    0: "NOTE_REST",
    48: "NOTE_C3", 50: "NOTE_D3", 52: "NOTE_E3", 53: "NOTE_F3", 55: "NOTE_G3", 57: "NOTE_A3", 59: "NOTE_B3",
    60: "NOTE_C4", 62: "NOTE_D4", 64: "NOTE_E4", 65: "NOTE_F4", 67: "NOTE_G4", 69: "NOTE_A4", 71: "NOTE_B4",
    72: "NOTE_C5", 74: "NOTE_D5", 76: "NOTE_E5", 77: "NOTE_F5", 79: "NOTE_G5", 81: "NOTE_A5", 83: "NOTE_B5"
}

class MIDIParser:
    def __init__(self, filename):
        self.filename = filename
        self.data = None
        self.tracks = []
        self.ticks_per_quarter = 480
        self.tempo = 500000  # microseconds per quarter note (120 BPM)
        
    def read_file(self):
        """อ่านไฟล์ MIDI"""
        try:
            with open(self.filename, 'rb') as f:
                self.data = f.read()
            return True
        except Exception as e:
            print(f"Error reading file {self.filename}: {e}")
            return False
    
    def parse_header(self):
        """แปลง MIDI header"""
        if len(self.data) < 14:
            return False
            
        # Check MIDI header
        if self.data[:4] != b'MThd':
            print("Not a valid MIDI file")
            return False
            
        header_length = struct.unpack('>I', self.data[4:8])[0]
        if header_length != 6:
            print("Unsupported MIDI header length")
            return False
            
        format_type = struct.unpack('>H', self.data[8:10])[0] 
        track_count = struct.unpack('>H', self.data[10:12])[0]
        time_division = struct.unpack('>H', self.data[12:14])[0]
        
        if format_type > 1:
            print(f"Unsupported MIDI format: {format_type}")
            return False
            
        self.ticks_per_quarter = time_division
        print(f"MIDI Format: {format_type}, Tracks: {track_count}, Ticks: {self.ticks_per_quarter}")
        
        return True
    
    def read_variable_length(self, offset):
        """อ่าน variable length value"""
        value = 0
        i = 0
        while i < 4:  # Maximum 4 bytes
            if offset + i >= len(self.data):
                break
            byte = self.data[offset + i]
            value = (value << 7) | (byte & 0x7F)
            i += 1
            if (byte & 0x80) == 0:
                break
        return value, i
    
    def parse_tracks(self):
        """แปลง MIDI tracks"""
        offset = 14  # After header
        
        while offset < len(self.data):
            # Track header
            if offset + 8 > len(self.data):
                break
                
            track_header = self.data[offset:offset+4]
            if track_header != b'MTrk':
                print(f"Invalid track header at offset {offset}")
                break
                
            track_length = struct.unpack('>I', self.data[offset+4:offset+8])[0]
            track_data = self.data[offset+8:offset+8+track_length]
            
            events = self.parse_track_events(track_data)
            self.tracks.append(events)
            
            offset += 8 + track_length
            
        return len(self.tracks) > 0
    
    def parse_track_events(self, track_data):
        """แปลง events ใน track"""
        events = []
        offset = 0
        current_time = 0
        running_status = 0
        
        while offset < len(track_data):
            # Read delta time
            delta_time, delta_bytes = self.read_variable_length(offset)
            offset += delta_bytes
            current_time += delta_time
            
            if offset >= len(track_data):
                break
                
            # Read event
            status = track_data[offset]
            
            # Handle running status
            if status < 0x80:
                status = running_status
                offset -= 1
            else:
                running_status = status
                
            offset += 1
            
            # Parse event based on status
            if 0x80 <= status <= 0xEF:  # Channel messages
                channel = status & 0x0F
                command = status & 0xF0
                
                if command == 0x90:  # Note On
                    if offset + 2 <= len(track_data):
                        note = track_data[offset]
                        velocity = track_data[offset + 1]
                        offset += 2
                        
                        # Convert to milliseconds
                        time_ms = int((current_time * 60000) / (self.ticks_per_quarter * 120))  # Assume 120 BPM
                        
                        if velocity > 0:
                            events.append({
                                'type': 'note_on',
                                'time': time_ms,
                                'note': note,
                                'velocity': velocity,
                                'channel': channel
                            })
                        else:
                            events.append({
                                'type': 'note_off', 
                                'time': time_ms,
                                'note': note,
                                'channel': channel
                            })
                            
                elif command == 0x80:  # Note Off
                    if offset + 2 <= len(track_data):
                        note = track_data[offset]
                        velocity = track_data[offset + 1]
                        offset += 2
                        
                        time_ms = int((current_time * 60000) / (self.ticks_per_quarter * 120))
                        events.append({
                            'type': 'note_off',
                            'time': time_ms, 
                            'note': note,
                            'channel': channel
                        })
                        
                else:
                    # Skip other channel messages
                    if command in [0xC0, 0xD0]:  # Program change, channel pressure
                        offset += 1
                    else:  # Other 2-byte messages
                        offset += 2
                        
            elif status == 0xFF:  # Meta events
                if offset < len(track_data):
                    meta_type = track_data[offset]
                    offset += 1
                    
                    length, length_bytes = self.read_variable_length(offset)
                    offset += length_bytes
                    
                    if meta_type == 0x51 and length == 3:  # Set tempo
                        tempo_data = track_data[offset:offset+3]
                        self.tempo = struct.unpack('>I', b'\x00' + tempo_data)[0]
                        print(f"Tempo: {60000000 // self.tempo} BPM")
                        
                    offset += length
                    
            else:
                # Skip unknown events
                break
                
        return events

def convert_to_orchestra_parts(tracks, part_count=4):
    """แปลง MIDI tracks เป็น Orchestra parts"""
    parts = [[] for _ in range(part_count)]
    
    # รวม events จากทุก tracks
    all_events = []
    for track in tracks:
        all_events.extend(track)
    
    # เรียงตามเวลา
    all_events.sort(key=lambda x: x['time'])
    
    # แบ่ง events ไปยัง parts
    note_states = {}  # track active notes
    
    for event in all_events:
        if event['type'] == 'note_on':
            # หาว่าควรใส่ใน part ไหน
            part_idx = event['channel'] % part_count
            
            # แปลงเป็น format ของ Orchestra
            note_name = NOTE_NAMES.get(event['note'], f"{event['note']}")
            
            # หา duration โดยมองหา note_off
            duration = 500  # default 500ms
            for future_event in all_events:
                if (future_event['type'] == 'note_off' and 
                    future_event['note'] == event['note'] and
                    future_event['time'] > event['time']):
                    duration = future_event['time'] - event['time']
                    break
            
            # เพิ่มเข้าไปใน part
            parts[part_idx].append({
                'note': note_name,
                'duration': max(100, duration),  # minimum 100ms
                'delay': 0  # จะคำนวณภายหลัง
            })
            
    # คำนวณ delay ระหว่างโน๊ต
    for part in parts:
        if len(part) > 1:
            for i in range(len(part) - 1):
                # ใช้ default delay สำหรับตอนนี้
                part[i]['delay'] = 100
        if part:
            part[-1]['delay'] = 0  # Last note no delay
            
    return parts

def generate_cpp_code(parts, song_name, song_id, tempo_bpm=120):
    """สร้างโค้ด C++ สำหรับ Orchestra"""
    
    cpp_code = f"""// Generated Orchestra Song: {song_name}
// Song ID: {song_id}
// Tempo: {tempo_bpm} BPM
// Parts: {len(parts)}

"""
    
    # Generate each part
    for i, part in enumerate(parts):
        if not part:  # Skip empty parts
            continue
            
        part_name = ['melody', 'harmony', 'bass', 'rhythm'][i]
        cpp_code += f"// Part {chr(65+i)}: {part_name.title()}\n"
        cpp_code += f"static const note_event_t {song_name.lower()}_{part_name}[] = {{\n"
        
        for note_event in part:
            note = note_event['note']
            duration = note_event['duration']
            delay = note_event['delay']
            
            cpp_code += f"    {{{note}, {duration}, {delay}}},\n"
        
        cpp_code += f"    {{NOTE_REST, 0, 0}}     // End\n"
        cpp_code += "};\n\n"
    
    # Generate parts array
    cpp_code += f"// {song_name} Parts Array\n"
    cpp_code += f"static const song_part_t {song_name.lower()}_parts[] = {{\n"
    
    for i, part in enumerate(parts):
        if not part:
            continue
        part_name = ['melody', 'harmony', 'bass', 'rhythm'][i]
        cpp_code += f"    {{{song_name.lower()}_{part_name}, "
        cpp_code += f"sizeof({song_name.lower()}_{part_name})/sizeof(note_event_t) - 1, "
        cpp_code += f'"{part_name.title()}"}},\n'
    
    cpp_code += "};\n\n"
    
    return cpp_code

def main():
    parser = argparse.ArgumentParser(description='Convert MIDI files to ESP32 Orchestra format')
    parser.add_argument('input', help='Input MIDI file')
    parser.add_argument('output', help='Output C++ header file')
    parser.add_argument('--song-name', default='CustomSong', help='Song name for code generation')
    parser.add_argument('--song-id', type=int, default=10, help='Song ID number')
    parser.add_argument('--tempo', type=int, default=120, help='Tempo in BPM')
    parser.add_argument('--parts', type=int, default=4, help='Number of orchestra parts')
    
    args = parser.parse_args()
    
    # Check input file
    if not Path(args.input).exists():
        print(f"Error: Input file '{args.input}' not found")
        return 1
    
    print(f"🎵 Converting {args.input} to Orchestra format...")
    
    # Parse MIDI file
    parser = MIDIParser(args.input)
    
    if not parser.read_file():
        return 1
        
    if not parser.parse_header():
        return 1
        
    if not parser.parse_tracks():
        print("Error: Could not parse MIDI tracks")
        return 1
    
    print(f"✅ Parsed {len(parser.tracks)} tracks")
    
    # Convert to Orchestra parts
    parts = convert_to_orchestra_parts(parser.tracks, args.parts)
    
    print(f"🎼 Generated {len(parts)} orchestra parts:")
    for i, part in enumerate(parts):
        print(f"   Part {chr(65+i)}: {len(part)} notes")
    
    # Generate C++ code
    cpp_code = generate_cpp_code(parts, args.song_name, args.song_id, args.tempo)
    
    # Write output file
    try:
        with open(args.output, 'w') as f:
            f.write("#ifndef GENERATED_SONG_H\n")
            f.write("#define GENERATED_SONG_H\n\n")
            f.write('#include "orchestra_common.h"\n\n')
            f.write(cpp_code)
            f.write("\n#endif // GENERATED_SONG_H\n")
        
        print(f"✅ Generated C++ code: {args.output}")
        print("\n📝 To use this song:")
        print(f'1. Add #include "{Path(args.output).name}" to your conductor code')
        print(f"2. Add the song to your all_songs[] array")
        print(f"3. Flash updated code to your ESP32 Orchestra")
        
    except Exception as e:
        print(f"Error writing output file: {e}")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())