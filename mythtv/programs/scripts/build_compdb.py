#! /usr/bin/python3

from pathlib import Path
import json
from optparse import OptionParser

parser = OptionParser()
parser.add_option("-v", "--verbose",
                  action="store_true", dest="verbose",
                  help="Print progress messages")
(options, args) = parser.parse_args()

paths = list(Path("..").glob("**/*.o.json"))
if len(paths) < 600:
    print("Not enough *.o.json files present.  Did you remember to")
    print("clean ccache and do a full build with clang?")
    exit(1)

outlines = []
for path in paths:
    filename = path.as_posix()
    badnames = ["/external/", "moc_", "dummy.o.json"]
    if any(bad in filename for bad in badnames):
        if options.verbose:
            print("Skipping input file: {0:s}".format(path.as_posix()))
        continue
    in_file = open(path, "r")
    if not in_file:
        if options.verbose:
            print("Cannot open input file: {0:s}".format(filename))
        continue

    # Read and strip trailing comma
    rawcontent = in_file.read()
    if len(rawcontent) < 2:
        print("Bad file contents: {0:s}".format(filename))
        exit(1)
    content = json.loads(rawcontent[:-2])
    in_file.close()

    # Manipulate file names
    if content["file"].startswith("../") and not content["file"].endswith("Json.cpp"):
        if options.verbose:
            print("Skipping input file2: {0:s}".format(filename))
        continue
    content["file"] = content["directory"] + "/" + content["file"]

    # Save the output for this file
    if options.verbose:
        print("Including input file: {0:s}".format(path.as_posix()))
    outlines.append(json.dumps(content, indent=4))

# Dump out the compdb file
out_file = open("../compile_commands.json", "w")
print("[\n{0:s}\n]".format(",\n".join(outlines)), file=out_file)
out_file.close()
