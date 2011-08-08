#!/bin/sh
#Make sure we built GPT
./build_gpt.pl

VERSION=`cat fait_accompli/version`
INSTALLER=gt$VERSION-all-source-installer
GPT=gpt*.tar.gz
CVSROOT=cvs.globus.org:/home/globdev/CVS/globus-packages

#GT5 bundles
BUNDLES=globus-resource-management-server,globus-resource-management-client,globus-resource-management-sdk,globus-data-management-server,globus-data-management-client,globus-data-management-sdk,globus-xio-extra-drivers,globus-rls-server,prews-test,globus-gsi,gsi_openssh_bundle,globus-gsi-test,gram5-condor,gram5-lsf,gram5-pbs,cas_callout

PACKAGES=myproxy

echo Making configure/make installer

echo Step: Checking out source code.
tag=""
user=""
while getopts "t:u:af:" arg; do
    case "$arg" in
        t)
	    tag="$OPTARG"
	    ;;
        u)
	    user="$OPTARG"
	    ;;
        a)
            user=":pserver:anonymous"
            ;;
        f)
            flavor="$OPTARG"
            ;;
    esac
done

./checkout-specs.pl -f etc/package-list-5.1.0 ${tag:+-t "$tag"} ${user:+-u "$user"}
if [ $? -ne 0 ]; then
	echo There was trouble checking out sources
	exit 8
fi

if [ "X$BRANCH" != "X" ]; then
    echo Step: Updating source with branch $BRANCH.
    mkdir tmp-branch
    cd tmp-branch
    cvs -Q co -r $BRANCH all
    cd ..
    cp -R tmp-branch/* source-trees/
    rm -rf tmp-branch
    INSTALLER=gt$BRANCH-all-source-installer
fi

if [ -d scripts ]; then
   echo
   echo "Step: Running Scripts..."
   for SCRIPT in `ls scripts 2>/dev/null`; do
       echo "Running $SCRIPT"
       scripts/$SCRIPT 
       if [ $? -ne 0 ]; then
           echo There was trouble running scripts/$SCRIPT
           exit 16
       fi
   done
   echo
fi

if [ -d patches ]; then
   echo
   echo "Step: Patching..."
   for PATCH in `ls patches 2>/dev/null`; do
       echo "Applying $PATCH"
       cat patches/$PATCH | patch -p0
       if [ $? -ne 0 ]; then
           echo There was trouble applying patches/$PATCH
           exit 16
       fi
   done
   echo
fi

echo "Step: Creating installer Makefile and bootstrapping."
./installer_creation.pl ${flavor:+-f "$flavor"}

if [ $? -ne 0 ]; then
	echo There was trouble making the installer.
	exit 1
fi

echo Bootstrapping done, about to copy source trees into installer.
echo This may take a few minutes.

mkdir $INSTALLER
cat fait_accompli/installer.Makefile.prelude fait_accompli/makefile_bundle_target.frag installer_makefile.frag > $INSTALLER/Makefile.in

sed -e "s/@version@/$VERSION/g" fait_accompli/installer.configure.in > farfleblatt2
autoconf farfleblatt2 > $INSTALLER/configure
chmod +x $INSTALLER/configure
cp fait_accompli/install-sh $INSTALLER
cp fait_accompli/config.sub $INSTALLER
cp fait_accompli/config.guess $INSTALLER
cp fait_accompli/config.site.in $INSTALLER
sed -e "s/@version@/$VERSION/g" fait_accompli/installer.INSTALL > $INSTALLER/INSTALL
sed -e "s/@version@/$VERSION/g" fait_accompli/installer.README > $INSTALLER/README

# untar GPT into the installer dir
tar -C $INSTALLER -xzf $GPT
# installer wants gpt to not be in a versioned directory
mv $INSTALLER/gpt-* $INSTALLER/gpt

# copy quickstart into the installer dir
cp -r quickstart $INSTALLER

# Symlink over the bootstrapped CVS dirs.
# Must use -h in tar command to dereference them
mkdir $INSTALLER/source-trees
CPOPTS=RpL

cp -${CPOPTS} source-trees/* $INSTALLER/source-trees
#rm -fr $INSTALLER/source-trees/autotools

echo Done creating installer.
