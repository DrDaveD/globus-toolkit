%if "%{?rhel}" == "5"
%global docdiroption "with-docdir"
%else
%global docdiroption "docdir"
%endif

%{!?perl_vendorlib: %global perl_vendorlib %(eval "`perl -V:installvendorlib`"; echo $installvendorlib)}

Name:		globus-gram-job-manager-condor
%global _name %(tr - _ <<< %{name})
Version:	0.2
Release:	1%{?dist}
Summary:	Globus Toolkit - Condor Job Manager

Group:		Applications/Internet
License:	ASL 2.0
URL:		http://www.globus.org/
Source:		%{_name}-%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Obsoletes:      globus-gram-job-manager-setup-condor < 4.5
Requires:	globus-gram-job-manager-scripts >= 3.4
Requires:	globus-gass-cache-program >= 2
Requires:	globus-common-progs >= 2
Requires:       condor
Requires:	perl(:MODULE_COMPAT_%(eval "`perl -V:version`"; echo $version))
Requires(post): globus-gram-job-manager-scripts >= 3.4
Requires(preun): globus-gram-job-manager-scripts >= 3.4
Provides:       globus-gram-job-manager-setup
BuildRequires:	grid-packaging-tools
BuildRequires:	globus-core
BuildRequires:	doxygen
BuildRequires:	graphviz
%if "%{?rhel}" == "5"
BuildRequires:	graphviz-gd
%endif
BuildRequires:	ghostscript
%if %{?fedora}%{!?fedora:0} >= 9 || %{?rhel}%{!?rhel:0} >= 6
BuildRequires:	tex(latex)
%else
BuildRequires:	tetex-latex
%endif

%description
The Globus Toolkit is an open source software toolkit used for building Grid
systems and applications. It is being developed by the Globus Alliance and
many others all over the world. A growing number of projects and companies are
using the Globus Toolkit to unlock the potential of grids for their cause.

The %{name} package contains:
Condor Job Manager 

%prep
%setup -q -n %{_name}-%{version}

%build
# Remove files that should be replaced during bootstrap
rm -f doxygen/Doxyfile*
rm -f doxygen/Makefile.am
rm -f pkgdata/Makefile.am
rm -f globus_automake*
rm -rf autom4te.cache

%{_datadir}/globus/globus-bootstrap.sh

export CONDOR_RM=/usr/bin/condor_rm
export CONDOR_SUBMIT=/usr/bin/condor_submit
%configure --%{docdiroption}=%{_docdir}/%{name}-%{version}

make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
# Remove jobmanager-condor from install dir so that it can be
# added/removed by post scripts
rm $RPM_BUILD_ROOT/etc/grid-services/jobmanager-condor

GLOBUSPACKAGEDIR=$RPM_BUILD_ROOT%{_datadir}/globus/packages

# Generate package filelists
cat $GLOBUSPACKAGEDIR/%{_name}/noflavor_data.filelist \
    $GLOBUSPACKAGEDIR/%{_name}/noflavor_doc.filelist \
    $GLOBUSPACKAGEDIR/%{_name}/noflavor_rtl.filelist \
  | sed s!^!%{_prefix}! \
  | sed s!^%{_prefix}/etc!/etc! > package.filelist

%clean
rm -rf $RPM_BUILD_ROOT

%post
if [ $1 -ge 1 ]; then
    globus-gatekeeper-admin -e jobmanager-condor > /dev/null 2>&1 || :
    if [ ! -f /etc/grid-services/jobmanager ]; then
        globus-gatekeeper-admin -e jobmanager-condor -n jobmanager
    fi
fi

%preun
if [ $1 -eq 0 ]; then
    globus-gatekeeper-admin -d jobmanager-condor > /dev/null 2>&1 || :
fi

%postun
if [ $i -eq 0 -a ! -f /etc/grid-services/jobmanager ]; then
    globus-gatekeeper-admin -E > /dev/null 2>&1 || :
fi

%files -f package.filelist
%defattr(-,root,root,-)
%dir %{_datadir}/globus/packages/%{_name}
%dir %{_docdir}/%{name}-%{version}
%config(noreplace) %{_sysconfdir}/grid-services/available/jobmanager-condor

%changelog
