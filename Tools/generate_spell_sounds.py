"""
AoC Spell Sound Generator - Creates all WAV files using only Python stdlib
Run this on Brad's PC via UE5 Python or standalone Python 3.8+
Generates sounds in: C:\Users\Bradh\Documents\Unreal Projects\AOC\Content\AoC\Sounds\
"""

import wave
import struct
import math
import os
import random

SAMPLE_RATE = 44100
CHANNELS = 1
SAMPLE_WIDTH = 2  # 16-bit

def make_sound(filename, duration, gen_func):
    """Write a WAV file from a generator function"""
    n_frames = int(SAMPLE_RATE * duration)
    data = b''
    for i in range(n_frames):
        t = i / SAMPLE_RATE
        sample = gen_func(t, duration)
        sample = max(-1.0, min(1.0, sample))
        data += struct.pack('<h', int(sample * 32767))
    
    with wave.open(filename, 'w') as wf:
        wf.setnchannels(CHANNELS)
        wf.setsampwidth(SAMPLE_WIDTH)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(data)

def envelope(t, dur, attack=0.05, release=0.1):
    """ADSR-like envelope"""
    if t < attack:
        return t / attack
    elif t > dur - release:
        return max(0, (dur - t) / release)
    return 1.0

def noise():
    return random.uniform(-1, 1)

# =============================================================================
# SCHOOL CAST SOUNDS (10)
# =============================================================================

def cast_arcana(t, d):
    env = envelope(t, d, 0.1, 0.2)
    # Mystical shimmer - layered sine waves with slight detuning
    return env * 0.6 * (
        math.sin(2 * math.pi * 440 * t) * 0.3 +
        math.sin(2 * math.pi * 554 * t) * 0.2 +
        math.sin(2 * math.pi * 660 * t * (1 + 0.01 * math.sin(5 * t))) * 0.3 +
        math.sin(2 * math.pi * 880 * t) * 0.1
    )

def cast_pyromancy(t, d):
    env = envelope(t, d, 0.05, 0.15)
    # Crackling fire charge - noise filtered with rising pitch
    freq = 200 + 600 * (t / d)
    return env * 0.5 * (
        noise() * 0.4 * math.sin(2 * math.pi * freq * t) +
        math.sin(2 * math.pi * freq * 0.5 * t) * 0.3 +
        noise() * 0.2
    )

def cast_cryomancy(t, d):
    env = envelope(t, d, 0.08, 0.3)
    # Crystalline cold - high frequency shimmer with glass-like resonance
    return env * 0.5 * (
        math.sin(2 * math.pi * 1200 * t) * 0.3 +
        math.sin(2 * math.pi * 2400 * t * (1 + 0.02 * math.sin(8 * t))) * 0.2 +
        math.sin(2 * math.pi * 600 * t) * 0.2 +
        noise() * 0.1 * math.sin(2 * math.pi * 3000 * t)
    )

def cast_stormcalling(t, d):
    env = envelope(t, d, 0.02, 0.1)
    # Electric buildup crackle
    crackle = noise() * math.sin(2 * math.pi * 100 * t) if random.random() > 0.7 else 0
    return env * 0.6 * (
        math.sin(2 * math.pi * 80 * t) * 0.3 +
        crackle * 0.5 +
        noise() * 0.2 * (t / d)
    )

def cast_necromancy(t, d):
    env = envelope(t, d, 0.15, 0.3)
    # Dark whispers with eerie low drone
    vibrato = math.sin(2 * math.pi * 3 * t) * 0.1
    return env * 0.5 * (
        math.sin(2 * math.pi * 80 * (1 + vibrato) * t) * 0.4 +
        math.sin(2 * math.pi * 120 * t) * 0.2 +
        noise() * 0.15 * math.sin(2 * math.pi * 200 * t) +
        math.sin(2 * math.pi * 40 * t) * 0.2
    )

def cast_verdancy(t, d):
    env = envelope(t, d, 0.1, 0.25)
    # Nature rustling - soft with organic harmonics
    return env * 0.4 * (
        math.sin(2 * math.pi * 330 * t) * 0.3 +
        math.sin(2 * math.pi * 440 * t * (1 + 0.005 * math.sin(2 * t))) * 0.3 +
        noise() * 0.15 +
        math.sin(2 * math.pi * 220 * t) * 0.2
    )

def cast_umbramancy(t, d):
    env = envelope(t, d, 0.2, 0.3)
    # Dark whisper gathering - low resonance with void-like quality
    return env * 0.5 * (
        math.sin(2 * math.pi * 60 * t) * 0.4 +
        math.sin(2 * math.pi * 90 * t * (1 + 0.05 * math.sin(1.5 * t))) * 0.3 +
        noise() * 0.1 * math.sin(2 * math.pi * 150 * t)
    )

def cast_radiance(t, d):
    env = envelope(t, d, 0.08, 0.2)
    # Warm holy chime - bright harmonics
    return env * 0.5 * (
        math.sin(2 * math.pi * 523 * t) * 0.3 +
        math.sin(2 * math.pi * 659 * t) * 0.25 +
        math.sin(2 * math.pi * 784 * t) * 0.2 +
        math.sin(2 * math.pi * 1046 * t) * 0.1
    )

def cast_sangromancy(t, d):
    env = envelope(t, d, 0.05, 0.2)
    # Sickly pulse - heartbeat-like with dark overtones
    beat = math.sin(2 * math.pi * 1.5 * t) ** 8  # Sharp pulses
    return env * 0.5 * (
        math.sin(2 * math.pi * 100 * t) * 0.3 * (1 + beat) +
        math.sin(2 * math.pi * 150 * t) * 0.2 +
        noise() * 0.1
    )

def cast_dominion(t, d):
    env = envelope(t, d, 0.1, 0.25)
    # Psychic hum - binaural-like beating
    return env * 0.5 * (
        math.sin(2 * math.pi * 200 * t) * 0.3 +
        math.sin(2 * math.pi * 204 * t) * 0.3 +  # 4Hz binaural beat
        math.sin(2 * math.pi * 400 * t) * 0.15
    )

# =============================================================================
# SPELL EFFECT SOUNDS (school-specific generators)
# =============================================================================

def fire_effect(t, d, intensity=1.0):
    env = envelope(t, d, 0.02, 0.1)
    return env * 0.6 * intensity * (
        noise() * 0.4 +
        math.sin(2 * math.pi * (150 + noise() * 50) * t) * 0.3 +
        noise() * 0.3 * math.sin(2 * math.pi * 300 * t)
    )

def ice_effect(t, d, intensity=1.0):
    env = envelope(t, d, 0.01, 0.15)
    crack = 0.8 if (t * 20) % 1 < 0.05 else 0  # Ice crack transients
    return env * 0.5 * intensity * (
        math.sin(2 * math.pi * 2000 * t) * 0.2 +
        math.sin(2 * math.pi * 3000 * t * (1 + 0.03 * math.sin(10 * t))) * 0.2 +
        noise() * 0.2 +
        crack * noise()
    )

def lightning_effect(t, d, intensity=1.0):
    env = envelope(t, d, 0.005, 0.05)
    # Sharp transient + rolling thunder
    transient = 1.0 if t < 0.02 else 0
    rumble = math.sin(2 * math.pi * 40 * t) * 0.3 * max(0, 1 - t / d)
    return env * 0.7 * intensity * (
        transient * noise() * 0.8 +
        rumble +
        noise() * 0.2 * max(0, 1 - t * 3)
    )

def necro_effect(t, d, intensity=1.0):
    env = envelope(t, d, 0.1, 0.2)
    vibrato = math.sin(2 * math.pi * 5 * t) * 0.15
    return env * 0.5 * intensity * (
        math.sin(2 * math.pi * 70 * (1 + vibrato) * t) * 0.4 +
        math.sin(2 * math.pi * 105 * t) * 0.2 +
        noise() * 0.2 * math.sin(2 * math.pi * 180 * t) +
        math.sin(2 * math.pi * 35 * t) * 0.2
    )

def nature_effect(t, d, intensity=1.0):
    env = envelope(t, d, 0.05, 0.2)
    return env * 0.4 * intensity * (
        math.sin(2 * math.pi * 300 * t * (1 + 0.01 * math.sin(3 * t))) * 0.3 +
        noise() * 0.25 +
        math.sin(2 * math.pi * 200 * t) * 0.2
    )

def shadow_effect(t, d, intensity=1.0):
    env = envelope(t, d, 0.08, 0.25)
    return env * 0.5 * intensity * (
        math.sin(2 * math.pi * 50 * t * (1 + 0.1 * math.sin(2 * t))) * 0.4 +
        noise() * 0.15 +
        math.sin(2 * math.pi * 75 * t) * 0.2
    )

def holy_effect(t, d, intensity=1.0):
    env = envelope(t, d, 0.03, 0.2)
    return env * 0.5 * intensity * (
        math.sin(2 * math.pi * 523 * t) * 0.25 +
        math.sin(2 * math.pi * 784 * t) * 0.2 +
        math.sin(2 * math.pi * 1046 * t) * 0.15 +
        math.sin(2 * math.pi * 1318 * t) * 0.1
    )

def blood_effect(t, d, intensity=1.0):
    env = envelope(t, d, 0.02, 0.15)
    pulse = abs(math.sin(2 * math.pi * 2 * t))
    return env * 0.5 * intensity * (
        math.sin(2 * math.pi * 100 * t) * 0.3 * pulse +
        noise() * 0.2 * pulse +
        math.sin(2 * math.pi * 60 * t) * 0.2
    )

def mind_effect(t, d, intensity=1.0):
    env = envelope(t, d, 0.05, 0.2)
    beat = math.sin(2 * math.pi * 3 * t)
    return env * 0.5 * intensity * (
        math.sin(2 * math.pi * 200 * t) * 0.3 +
        math.sin(2 * math.pi * 203 * t) * 0.3 +
        math.sin(2 * math.pi * 400 * t * (1 + 0.02 * beat)) * 0.15
    )

def arcane_effect(t, d, intensity=1.0):
    env = envelope(t, d, 0.03, 0.15)
    return env * 0.5 * intensity * (
        math.sin(2 * math.pi * 440 * t) * 0.25 +
        math.sin(2 * math.pi * 550 * t * (1 + 0.01 * math.sin(6 * t))) * 0.2 +
        math.sin(2 * math.pi * 880 * t) * 0.15 +
        noise() * 0.1
    )


def main():
    base_dir = r'C:\Users\Bradh\Documents\Unreal Projects\AOC\Content\AoC\Sounds'
    cast_dir = os.path.join(base_dir, 'Cast')
    effect_dir = os.path.join(base_dir, 'Effects')
    
    os.makedirs(cast_dir, exist_ok=True)
    os.makedirs(effect_dir, exist_ok=True)
    
    print("=== Generating Cast Sounds ===")
    cast_sounds = [
        ('cast_arcana', cast_arcana, 1.0),
        ('cast_pyromancy', cast_pyromancy, 0.8),
        ('cast_cryomancy', cast_cryomancy, 1.0),
        ('cast_stormcalling', cast_stormcalling, 0.6),
        ('cast_necromancy', cast_necromancy, 1.2),
        ('cast_verdancy', cast_verdancy, 1.0),
        ('cast_umbramancy', cast_umbramancy, 1.2),
        ('cast_radiance', cast_radiance, 0.8),
        ('cast_sangromancy', cast_sangromancy, 0.8),
        ('cast_dominion', cast_dominion, 1.0),
    ]
    
    for name, func, duration in cast_sounds:
        path = os.path.join(cast_dir, f'{name}.wav')
        make_sound(path, duration, func)
        print(f'  {name}.wav ({duration}s)')
    
    print("\n=== Generating Effect Sounds ===")
    
    # Map of school -> (effect_func, spells_list)
    schools = {
        'pyromancy': (fire_effect, [
            ('flamethrower', 1.5), ('fire_explosion', 0.8), ('flame_wave', 1.0),
            ('pyroclasm', 1.2), ('meteor_bombardment', 2.0), ('firestorm', 2.5),
            ('living_flame_creature', 1.5), ('fire_shield', 1.0),
            ('fireball', 0.6), ('ignite', 0.8), ('fire_wall', 1.5),
            ('heat_wave', 1.0), ('inferno', 2.0), ('dragon_breath', 1.5),
            ('volcanic_eruption', 2.5), ('sun_strike', 1.0),
        ]),
        'cryomancy': (ice_effect, [
            ('ice_shard', 0.5), ('frostbolt', 0.6), ('ice_lance', 0.4),
            ('cold_snap', 0.3), ('frost_nova', 0.8), ('blizzard', 3.0),
            ('glacier', 1.5), ('absolute_zero', 2.0), ('deep_freeze', 1.0),
            ('ice_armor', 1.0), ('ice_storm', 2.5), ('frozen_ground', 1.5),
            ('ice_wall', 1.0), ('permafrost', 1.5), ('shatter', 0.5),
            ('avalanche', 2.0),
        ]),
        'stormcalling': (lightning_effect, [
            ('lightning_bolt', 0.4), ('chain_lightning', 0.8),
            ('thunder_clap', 0.6), ('storm_surge', 1.5), ('ball_lightning', 1.0),
            ('tempest_strike', 0.5), ('static_field', 1.2), ('storm_shield', 1.0),
            ('thunderstorm', 2.5), ('lightning_storm', 2.0),
            ('overcharge', 0.8), ('arc_lightning', 0.6),
            ('electromagnetic_pulse', 1.0), ('mjolnir', 0.8),
            ('zeus_bolt', 0.5), ('storm_of_ages', 3.0),
        ]),
        'necromancy': (necro_effect, [
            ('death_coil', 0.5), ('necrotic_strike', 0.4),
            ('bone_armor', 1.0), ('soul_drain', 1.5), ('raise_dead', 2.0),
            ('death_grip', 0.6), ('plague', 2.0), ('summon_ghoul', 1.5),
            ('corpse_explosion', 0.8), ('anti_magic_shell', 1.0),
            ('death_and_decay', 2.5), ('soul_harvest', 1.0),
            ('unholy_frenzy', 1.0), ('dark_transformation', 1.5),
            ('army_of_dead', 2.5), ('apocalypse', 3.0),
        ]),
        'verdancy': (nature_effect, [
            ('healing', 1.0), ('nature_blessing', 1.0), ('vine_whip', 0.5),
            ('root_slam', 0.6), ('thorn_barrage', 0.8), ('overgrowth', 1.5),
            ('natures_wrath', 2.0), ('stranglehold', 1.0),
            ('bark_skin', 1.0), ('entangle', 1.0), ('regrowth', 1.5),
            ('bloom', 0.8), ('poison_ivy', 1.0), ('treant_form', 1.5),
            ('forest_guardian', 2.0), ('world_tree', 2.5),
        ]),
        'umbramancy': (shadow_effect, [
            ('shadow_bolt', 0.5), ('void_rift', 1.0), ('dark_drain', 1.5),
            ('shadow_explosion', 0.8), ('creeping_shadow', 1.5),
            ('abyssal_gate', 2.0), ('shadow_cloak', 1.0), ('shadow_step', 0.4),
            ('darkness', 1.5), ('void_zone', 2.0), ('nightmare', 1.5),
            ('eclipse', 1.0), ('phantom_strike', 0.5), ('void_prison', 1.0),
            ('dark_ascension', 2.0), ('oblivion', 2.5),
        ]),
        'radiance': (holy_effect, [
            ('holy_bolt', 0.5), ('smite', 0.6), ('holy_nova', 0.8),
            ('divine_beam', 1.5), ('celestial_storm', 2.5),
            ('sanctuary', 1.5), ('radiance_blessing', 1.0), ('purify', 0.6),
            ('holy_shield', 1.0), ('judgment', 0.8), ('divine_wrath', 1.5),
            ('resurrection', 2.0), ('holy_fire', 1.0), ('exorcism', 0.8),
            ('archangel', 2.0), ('rapture', 2.5),
        ]),
        'sangromancy': (blood_effect, [
            ('blood_bolt', 0.5), ('blood_drain', 1.5), ('blood_boil', 1.0),
            ('blood_explosion', 0.8), ('dark_ritual', 1.5),
            ('heartbeat', 1.0), ('blood_ward', 1.0), ('hemorrhage', 0.6),
            ('blood_pact', 1.0), ('sanguine_shield', 1.0),
            ('crimson_tide', 1.5), ('blood_bath', 2.0),
            ('life_tap', 0.8), ('exsanguinate', 1.0),
            ('blood_curse', 1.5), ('blood_god', 2.5),
        ]),
        'dominion': (mind_effect, [
            ('force_push', 0.4), ('mind_shatter', 0.6), ('fear', 1.0),
            ('psychic_scream', 0.8), ('telekinesis', 1.0), ('doom', 2.0),
            ('psychic_shield', 1.0), ('mind_spike', 0.4),
            ('confusion', 1.0), ('dominate', 1.5), ('force_wave', 0.6),
            ('mind_crush', 0.8), ('mass_hysteria', 1.5),
            ('mind_plague', 2.0), ('thought_annihilation', 1.0),
            ('absolute', 2.5),
        ]),
        'arcana': (arcane_effect, [
            ('missile', 0.4), ('beam', 1.5), ('mana_burst', 0.6),
            ('rift', 1.0), ('dispel', 0.5), ('mana_shield', 1.0),
            ('arcane_barrage', 0.8), ('teleport', 0.5),
            ('counterspell', 0.4), ('arcane_explosion', 0.8),
            ('energy_powerup', 1.0), ('time_warp', 1.5),
            ('blink', 0.3), ('spell_steal', 0.6),
            ('arcane_torrent', 2.0), ('wish', 2.5),
        ]),
    }
    
    total = 0
    for school, (effect_func, spells) in schools.items():
        print(f'\n  [{school.upper()}]')
        for spell_name, duration in spells:
            filename = f'{school}_{spell_name}.wav'
            path = os.path.join(effect_dir, filename)
            # Vary intensity based on position in list (simulate tier)
            tier_idx = spells.index((spell_name, duration))
            intensity = 0.7 + (tier_idx / len(spells)) * 0.5
            make_sound(path, duration, lambda t, d, ef=effect_func, inten=intensity: ef(t, d, inten))
            total += 1
            print(f'    {filename} ({duration}s)')
    
    print(f'\n=== DONE: Generated {total} effect sounds + 10 cast sounds = {total + 10} total ===')


if __name__ == '__main__':
    main()
