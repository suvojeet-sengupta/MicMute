import struct

def create_icon(filename, color_r, color_g, color_b):
    # ICO Header
    # idReserved (2), idType (2), idCount (2)
    header = struct.pack('<HHH', 0, 1, 1)

    # Image Entry
    # bWidth (1), bHeight (1), bColorCount (1), bReserved (1)
    # wPlanes (2), wBitCount (2), dwBytesInRes (4), dwImageOffset (4)
    width = 16
    height = 16
    # 32-bit color typical requires 40 byte header + pixel data + mask
    # For simplicity, we'll create a raw BMP based icon logic or just a basic structure.
    # Actually, the easiest Valid ICO is just a BMP with a directory.
    # Let's generate a 16x16 32-bit uncompressed BMP based icon data.
    
    # BMP Info Header (40 bytes)
    # Size(4), Width(4), Height(4*2 for XOR+AND mask implied height), Planes(2), BitCount(2), Compression(4)...
    # Simple approach: 16x16 24bit.
    
    # Let's simple write a helper to make a basic 16x16 RED or GREEN square.
    
    # 1. Directory Entry
    w = 16
    h = 16
    bColorCount = 0
    bReserved = 0
    wPlanes = 1
    wBitCount = 32
    
    # Bitmap Header size (40) + Pixel Data (16*16*4) + AND Mask (16*16/8 = 32 bytes)
    # 40 + 1024 + 32 = 1096 bytes
    dwBytesInRes = 40 + (16*16*4) + (16*4 // 8 * 16) # Width bytes must be multiple of 4. 16*1bit = 2 bytes. 2*16 = 32 bytes.
    dwImageOffset = 22 # 6 (header) + 16 (entry)
    
    entry = struct.pack('<BBBBHHII', w, h, bColorCount, bReserved, wPlanes, wBitCount, dwBytesInRes, dwImageOffset)
    
    # DIB Header
    biSize = 40
    biWidth = 16
    biHeight = 16 * 2 # Height is doubled because it contains XOR and AND masks
    biPlanes = 1
    biBitCount = 32
    biCompression = 0
    biSizeImage = 0
    biXPelsPerMeter = 0
    biYPelsPerMeter = 0
    biClrUsed = 0
    biClrImportant = 0
    
    dib_header = struct.pack('<IiiHHIIIIII', biSize, biWidth, biHeight, biPlanes, biBitCount, biCompression, biSizeImage, biXPelsPerMeter, biYPelsPerMeter, biClrUsed, biClrImportant)
    
    # Pixel Data (Bottom-up)
    # 16x16 pixels, 4 bytes each (BGRA)
    pixels = bytearray()
    for y in range(16):
        for x in range(16):
            pixels.extend([color_b, color_g, color_r, 255])
            
    # AND Mask (1-bit per pixel, padded to 32-bit boundary)
    # 16 pixels = 2 bytes. Padded to 4 bytes.
    # 0 = Opaque, 1 = Transparent. We want all 0.
    mask_row = b'\x00\x00\x00\x00' 
    mask = bytearray()
    for y in range(16):
        mask.extend(mask_row)
        
    with open(filename, 'wb') as f:
        f.write(header)
        f.write(entry)
        f.write(dib_header)
        f.write(pixels)
        f.write(mask)

create_icon('resources/app.ico', 0, 0, 255) # Blue
create_icon('resources/mic_on.ico', 0, 255, 0) # Green
create_icon('resources/mic_off.ico', 255, 0, 0) # Red
