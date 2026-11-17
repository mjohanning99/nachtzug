#!/usr/bin/env python3
"""Writes the release notes into the AppStream file from CHANGELOG.md, so software centres show what each version
changed. Run it after adding a version's section to CHANGELOG.md (step 3 of the release steps):

    python3 tools/metainfo.py          writes the releases into assets/io.github.mjohanning99.nachtzug.metainfo.xml
    python3 tools/metainfo.py --check  only checks they are up to date, and fails if not

Each section "## x.y.z (17 November 2026)" becomes a <release>; its paragraphs and its list of points become the
release's description. **Bold** turns into emphasis, `code` into code, and [links](…) keep only their words.
"""
import datetime
import html
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CHANGELOG = ROOT / "CHANGELOG.md"
METAINFO = ROOT / "assets" / "io.github.mjohanning99.nachtzug.metainfo.xml"
HEADING = re.compile(r"^## (\d+\.\d+\.\d+) \((\d{1,2} \w+ \d{4})\)\s*$")


def inline(text):
	"""One line of the changelog as AppStream text: escaped, with emphasis and code, links reduced to their words."""
	text = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", text)
	text = html.escape(text, quote=False)
	text = re.sub(r"\*\*(.+?)\*\*", r"<em>\1</em>", text)
	return re.sub(r"`([^`]+)`", r"<code>\1</code>", text)


def releases():
	"""The changelog's versions, newest first: (version, ISO date, [description lines])."""
	found = []
	for line in CHANGELOG.read_text(encoding="utf-8").splitlines():
		m = HEADING.match(line)
		if not m and line.startswith("## "):
			# a version not dated yet, "## 0.3.5 (unreleased)": not ready to publish
			sys.exit(f"CHANGELOG.md: \"{line[3:]}\" needs its date, as in \"## 0.3.5 (18 November 2026)\"")
		if m:
			date = datetime.datetime.strptime(m.group(2), "%d %B %Y").date().isoformat()
			found.append((m.group(1), date, []))
		elif found and line.strip():
			found[-1][2].append(line.rstrip())
	return found


def describe(lines):
	"""A section's lines as AppStream markup: paragraphs as <p>, a run of "- " points as one <ul>."""
	out = []
	in_list = False
	for line in lines:
		if line.startswith("- "):
			if not in_list:
				out.append("<ul>")
				in_list = True
			out.append(f"  <li>{inline(line[2:])}</li>")
		elif line.startswith("  ") and in_list:  # a point that goes on over the next line
			out[-1] = out[-1][: -len("</li>")] + " " + inline(line.strip()) + "</li>"
		else:
			if in_list:
				out.append("</ul>")
				in_list = False
			out.append(f"<p>{inline(line)}</p>")
	if in_list:
		out.append("</ul>")
	return out


def block():
	"""The whole <releases> element, indented as it sits in the file."""
	parts = ["  <releases>"]
	for version, date, lines in releases():
		parts.append(f'    <release version="{version}" date="{date}">')
		parts.append("      <url type=\"details\">https://github.com/mjohanning99/nachtzug/releases/tag/v" + version + "</url>")
		parts.append("      <description>")
		parts += ["        " + line for line in describe(lines)]
		parts.append("      </description>")
		parts.append("    </release>")
	parts.append("  </releases>")
	return "\n".join(parts)


def main():
	text = METAINFO.read_text(encoding="utf-8")
	new = re.sub(r"  <releases>.*?</releases>", lambda _: block(), text, count=1, flags=re.S)
	if "--check" in sys.argv[1:]:
		if new != text:
			sys.exit(f"{METAINFO.relative_to(ROOT)}: the releases differ from CHANGELOG.md; run tools/metainfo.py")
		print("releases up to date")
		return
	METAINFO.write_text(new, encoding="utf-8")
	print(f"{len(releases())} releases written to {METAINFO.relative_to(ROOT)}")


if __name__ == "__main__":
	main()
