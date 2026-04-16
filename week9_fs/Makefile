# Week 9 — build slides (marp) and documents (pandoc) to PDF
# Usage:
#   make            # build all PDFs
#   make slides     # slides only
#   make docs       # documents only
#   make clean      # remove generated PDFs

MARP   := npx @marp-team/marp-cli
PANDOC := pandoc

# Marp slides sources (contain "marp: true" frontmatter)
SLIDE_SRC := week9_slides.md
SLIDE_PDF := $(SLIDE_SRC:.md=.pdf)

# Document sources (everything else, excluding README)
DOC_SRC := lab9_instructions.md lab9_report_template.md
DOC_PDF := $(DOC_SRC:.md=.pdf)

PANDOC_FLAGS := --pdf-engine=xelatex \
    -V geometry:margin=1in \
    -V fontsize=11pt \
    -V colorlinks=true \
    -V monofont="Menlo"

.PHONY: all slides docs clean

all: slides docs

slides: $(SLIDE_PDF)

docs: $(DOC_PDF)

%.pdf: %.md
	@if head -3 "$<" | grep -q 'marp: true'; then \
		echo "  MARP  $< -> $@"; \
		$(MARP) --pdf --allow-local-files "$<" -o "$@"; \
	else \
		echo "  PANDOC $< -> $@"; \
		$(PANDOC) $(PANDOC_FLAGS) -o "$@" "$<"; \
	fi

clean:
	rm -f $(SLIDE_PDF) $(DOC_PDF)
