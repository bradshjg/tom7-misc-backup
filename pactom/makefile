
# default : programs outputs thumbnails
# default : kmlcat
# default : shortest.kml shortest.svg
default : map map.svg
# default : llt.svg

# XXXXXXXXXXXXXXXXXXXXX
# NOTE! I commented out the dependencies on binaries because
# I cannot build mlton on Windows 10 in 2019 :(

all : programs outputs thumbnails pdfs obsolete release

PACTOMLIB=pactom.sml pactom-sig.sml ../sml-lib/files/xml/xml*.sml ../sml-lib/misc/color*.sml ../sml-lib/geom/latlon*.sml ../sml-lib/geom/snappoint*.sml ../sml-lib/geom/latlontree*.sml ../sml-lib/geom/quadtree*.sml ../sml-lib/geom/pointlocation*.sml ../sml-lib/data/undirected-graph*.sml ../sml-lib/util/growarray*.sml ../sml-lib/algo/heap*.sml

# has trouble on new cygwin, because it's a bash builtin?
TIME=

# '/c/program files (x86)/mlton/bin/mlton'
MLTON=/c/mlton/bin/mlton.bat -target x86_64-w64-mingw32 -codegen amd64

# Generating programs.
programs : shortestpaths colorize map elevsvg makepoints radial lltsvg kmlcat

kmlcat : kmlcat.sml kmlcat.cm ../sml-lib/util/*.sml
	${MLTON} kmlcat.cm

shortestpaths : shortestpaths.sml shortestpaths.cm ${PACTOMLIB} 
	${MLTON} shortestpaths.cm

colorize : colorize.sml colorize.cm ${PACTOMLIB}
	${MLTON} colorize.cm

map : map.sml map.cm ${PACTOMLIB}
	${MLTON} map.cm

# XXX need output for this
elevsvg : elevsvg.sml elevsvg.cm ${PACTOMLIB}
	${MLTON} elevsvg.cm

makepoints : makepoints.sml makepoints.cm ${PACTOMLIB} ../sml-lib/geom/*.sml
	${MLTON} makepoints.cm

radial : radial.sml radial.cm ${PACTOMLIB} ../sml-lib/geom/*.sml
	${MLTON} radial.cm

lltsvg : lltsvg.sml lltsvg.cm ${PACTOMLIB}
	${MLTON} lltsvg.cm

# outputs
# XXX these should explicitly depend on inputs, like pac.kml

outputs : radial.svg points.kml llt.svg map.svg

map.svg : pac.kml pacannotations.kml pac2.kml neighborhoods.kml interstate-logo.svg # map
	${TIME} ./map > $@

radial.svg : pac.kml pac2.kml # radial
	${TIME} ./radial > $@

points.kml : neighborhoods.kml # makepoints 
	${TIME} ./makepoints > $@

# This is getting near the maximum memory for 32 bit machines! Probably can do some
# kind of pre-thinning of the points, or maybe a more efficient kd-tree representation,
# or merge at load time?
# @MLton max-heap 600m --
shortest.kml shortest.svg :  pac.kml pac2.kml # shortestpaths
	${TIME} ./shortestpaths -kmlout shortest.kml -svgout shortest.svg

llt.svg :  pac.kml pac2.kml # lltsvg
	${TIME} ./lltsvg > $@


# MAKETHUMB=convert -scale 400
# MAKETHUMB="/c/program files/inkscape/inkscape"
MAKETHUMB=rsvg/rsvg-convert --keep-aspect-ratio --width=580
MAKEPDF=rsvg/rsvg-convert -f pdf

# XXX Text rendering is not that good this way. Might want to render at a much
# larger size then downsample with imagemagick?

# Convert SVGs to thumbnails for webpage
thumbnails : map.png shortest.png
pdfs : map.pdf shortest.pdf

map.png : map.svg
	${MAKETHUMB} $< -o $@

shortest.png : shortest.svg
	${MAKETHUMB} $< -o $@

radial.png : radial.svg
	${MAKETHUMB} $< -o $@

%.pdf : %.svg
	${MAKEPDF} $< -o $@

# Obsolete
obsolete : parsexml

parsexml : parsexml.cm parsexml.sml ${PACTOMLIB}
	${MLTON} parsexml.cm

# For release, we expect the following files in the current directory:
#   pac.kml
#   pac2.kml

RELEASEFILES=release/pactom.kmz release/shortest.svg release/map.svg release/map.png release/shortest.png release/shortest.pdf release/map.pdf release/radial.svg release/radial.pdf release/radial.png
release : ${RELEASEFILES} $(addsuffix .size, ${RELEASEFILES})
	date +"%d %b %Y at %H:%M" | tr -d '\012' > release/updated
upload : release
	scp -P 77 -r release root@spacebar.org:/var/www/pac/

release/%.png : %.png
	cp $< $@

release/%.svg : %.svg
	cp $< $@

release/%.pdf : %.pdf
	cp $< $@

# XXX also annotations, neighborhood, etc.
release/pactom.kmz : pac.kml pac2.kml # kmlcat
	rm -f _pactom.kml
	./kmlcat pac.kml pac2.kml > _pactom.kml
	rm -f release/pactom.kmz
	zip release/pactom.kmz _pactom.kml

# Produces a human-readable size (i.e. "2.7M") and strips newlines.
# These are used on the web page to report the size of downloadable
# files.
release/%.size : release/%
	du --si $^ | cut -f 1 | tr -d '\012' > $@

clean :
	rm -f _pactom.kml release/pactom.kmz release/*.size *~

# clean final generated files too, including the whole release dir
veryclean : clean
	rm -rf release
	mkdir release
	rm -f shortest.kml shortest.svg map.svg shortest.pdf map.pdf radial.svg points.kml radial.pdf kmlcat map shortestpaths colorize elevsvg makepoints radial lltsvg
