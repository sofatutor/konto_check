# Generated by jeweler
# DO NOT EDIT THIS FILE DIRECTLY
# Instead, edit Jeweler::Tasks in Rakefile, and run 'rake gemspec'
# -*- encoding: utf-8 -*-
# stub: konto_check 6.01.0 ruby lib
# stub: ext/konto_check_raw/extconf.rb

Gem::Specification.new do |s|
  s.name = "konto_check"
  s.version = "6.01.0"

  s.required_rubygems_version = Gem::Requirement.new(">= 0") if s.respond_to? :required_rubygems_version=
  s.require_paths = ["lib"]
  s.authors = ["Provideal Systems GmbH", "Jan Schwenzien", "Michael Plugge"]
  s.date = "2017-09-20"
  s.description = "Check whether a certain bic/account-no-combination or an IBAN can possibly be valid, generate IBANs, retrieve informations about a bank or search for BICs matching certain criteria. It uses the C library kontocheck (see http://sourceforge.net/projects/kontocheck/) by Michael Plugge."
  s.email = "m.plugge@hs-mannheim.de"
  s.extensions = ["ext/konto_check_raw/extconf.rb"]
  s.extra_rdoc_files = [
    "LICENSE",
    "README.textile",
    "ext/konto_check_raw/konto_check_raw_ruby.c"
  ]
  s.files = [
    "ext/konto_check_raw/extconf.rb",
    "ext/konto_check_raw/konto_check.c",
    "ext/konto_check_raw/konto_check.h",
    "ext/konto_check_raw/konto_check_raw_ruby.c",
    "ext/konto_check_raw/retvals.h",
    "lib/konto_check.rb"
  ]
  s.homepage = "http://kontocheck.sourceforge.net"
  s.rubygems_version = "2.5.2"
  s.summary = "Checking german BICs/Bank account numbers and IBANs, generate IBANs, retrieve information about german Banks, search for Banks matching certain criteria, check IBAN or BIC, convert bic/account to IBAN and BIC"

  if s.respond_to? :specification_version then
    s.specification_version = 4

    if Gem::Version.new(Gem::VERSION) >= Gem::Version.new('1.2.0') then
      s.add_development_dependency(%q<thoughtbot-shoulda>, [">= 0"])
    else
      s.add_dependency(%q<thoughtbot-shoulda>, [">= 0"])
    end
  else
    s.add_dependency(%q<thoughtbot-shoulda>, [">= 0"])
  end
end

