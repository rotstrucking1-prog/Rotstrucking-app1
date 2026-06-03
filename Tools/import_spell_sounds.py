"""
Import generated WAV files into UE5 as SoundWave assets.
Run this inside UE5's Python executor AFTER generate_spell_sounds.py.
"""
import unreal
import os

def import_sounds():
    base_dir = r'C:\Users\Bradh\Documents\Unreal Projects\AOC\Content\AoC\Sounds'
    cast_dir = os.path.join(base_dir, 'Cast')
    effect_dir = os.path.join(base_dir, 'Effects')
    
    at = unreal.AssetToolsHelpers.get_asset_tools()
    
    # Ensure UE5 directories exist
    unreal.EditorAssetLibrary.make_directory('/Game/AoC/Sounds/Cast')
    unreal.EditorAssetLibrary.make_directory('/Game/AoC/Sounds/Effects')
    
    total = 0
    
    # Import cast sounds
    if os.path.exists(cast_dir):
        wavs = [f for f in os.listdir(cast_dir) if f.endswith('.wav')]
        print(f'Importing {len(wavs)} cast sounds...')
        for wav in wavs:
            src = os.path.join(cast_dir, wav)
            name = wav.replace('.wav', '')
            dest = '/Game/AoC/Sounds/Cast'
            
            task = unreal.AssetImportTask()
            task.filename = src
            task.destination_path = dest
            task.destination_name = name
            task.replace_existing = True
            task.automated = True
            task.save = True
            
            at.import_asset_tasks([task])
            total += 1
            print(f'  Cast: {name}')
    
    # Import effect sounds
    if os.path.exists(effect_dir):
        wavs = [f for f in os.listdir(effect_dir) if f.endswith('.wav')]
        print(f'Importing {len(wavs)} effect sounds...')
        for wav in wavs:
            src = os.path.join(effect_dir, wav)
            name = wav.replace('.wav', '')
            dest = '/Game/AoC/Sounds/Effects'
            
            task = unreal.AssetImportTask()
            task.filename = src
            task.destination_path = dest
            task.destination_name = name
            task.replace_existing = True
            task.automated = True
            task.save = True
            
            at.import_asset_tasks([task])
            total += 1
            print(f'  Effect: {name}')
    
    print(f'\n=== Imported {total} sound assets into UE5 ===')
    
    # Save all
    unreal.EditorAssetLibrary.save_directory('/Game/AoC/Sounds')
    print('All sounds saved!')

import_sounds()
