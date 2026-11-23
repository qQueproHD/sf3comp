// Renders an image into an .icns. Usage: swift make_icns.swift IN OUT.icns
// IN .svg: drawn as it is (it is already icon-shaped: fork/icon/icon.svg). Any other image (PNG, JPEG, ...): cropped to
// fill macOS's rounded-square icon shape, with the standard margin.
import AppKit

let args = CommandLine.arguments
guard args.count == 3, let svg = NSImage(contentsOfFile: args[1]) else {
    FileHandle.standardError.write("usage: make_icns.swift IN OUT.icns\n".data(using: .utf8)!)
    exit(1)
}
let set = URL(fileURLWithPath: NSTemporaryDirectory()).appendingPathComponent("sf3comp-\(getpid()).iconset")
try? FileManager.default.removeItem(at: set)
try FileManager.default.createDirectory(at: set, withIntermediateDirectories: true)
for base in [16, 32, 128, 256, 512] {
    for scale in [1, 2] {
        let px = base * scale
        let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: px, pixelsHigh: px, bitsPerSample: 8,
                                   samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB,
                                   bytesPerRow: 0, bitsPerPixel: 0)!
        NSGraphicsContext.saveGraphicsState()
        NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
        if args[1].lowercased().hasSuffix(".svg") {
            svg.draw(in: NSRect(x: 0, y: 0, width: px, height: px))
        } else {
            // (Apple's grid: an 824/1024 rounded square, corner 186/1024; the picture scaled to fill it, centred)
            let unit = CGFloat(px) / 1024
            let shape = NSRect(x: 100 * unit, y: 100 * unit, width: 824 * unit, height: 824 * unit)
            NSBezierPath(roundedRect: shape, xRadius: 186 * unit, yRadius: 186 * unit).addClip()
            let fill = max(shape.width / svg.size.width, shape.height / svg.size.height)
            let w = svg.size.width * fill, h = svg.size.height * fill
            svg.draw(in: NSRect(x: shape.midX - w / 2, y: shape.midY - h / 2, width: w, height: h))
        }
        NSGraphicsContext.restoreGraphicsState()
        let name = scale == 1 ? "icon_\(base)x\(base).png" : "icon_\(base)x\(base)@2x.png"
        try rep.representation(using: .png, properties: [:])!.write(to: set.appendingPathComponent(name))
    }
}
let iconutil = Process()
iconutil.executableURL = URL(fileURLWithPath: "/usr/bin/iconutil")
iconutil.arguments = ["-c", "icns", set.path, "-o", args[2]]
try iconutil.run()
iconutil.waitUntilExit()
try? FileManager.default.removeItem(at: set)
exit(iconutil.terminationStatus)
