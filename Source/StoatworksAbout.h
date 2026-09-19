/*
 * Stoatworks Labs - About window data for Occluder.
 *
 * Written by hand in the shape stoatworks-backend/scripts/sync-about.py
 * produces, because the project is not registered on the website yet. Once it
 * is, the sync regenerates this file from projects.json and fills in the guide
 * and page links; until then those are empty and the panel leaves the rows out.
 *
 * `version` here is a fallback read from this repo's own manifest at sync
 * time. Anything with a build step injects the real one at build time and
 * overrides this.
 */
#pragma once

namespace stoatworks::about
{
    inline constexpr auto name = "Occluder";
    inline constexpr auto slug = "occluder";
    inline constexpr auto hook = "One knob from open ears to earplugs";
    inline constexpr auto licence = "MIT";
    inline constexpr auto guide = "";
    inline constexpr auto page = "";
    inline constexpr auto repo = "https://github.com/stoatworks-labs/occluder";
    inline constexpr auto versionFallback = "v0.1.0";

    inline constexpr auto org = "Stoatworks Labs";
    inline constexpr auto home = "https://stoatworks-labs.com";
    inline constexpr auto tagline = "Open tools for the people who run the show.";

    /* The canonical funding links, matching FUNDING.yml and the support footer. */
    struct Link { const char* name; const char* url; };
    inline constexpr Link funding[] = {
        { "GitHub Sponsors", "https://github.com/sponsors/stoatworks-labs" },
        { "Ko-fi", "https://ko-fi.com/stoatworkslabs" },
        { "Patreon", "https://patreon.com/StoatworksLabs" },
        { "Liberapay", "https://liberapay.com/stoatworks-labs" },
    };
}
