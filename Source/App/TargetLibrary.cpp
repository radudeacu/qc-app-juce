#include "TargetLibrary.h"

#include <BinaryData.h>

namespace qc
{
    namespace
    {
        bool readDouble (const juce::var& object, const char* key, double& destination)
        {
            if (! object.hasProperty (key))
                return false;

            const auto value = object[key];

            if (! value.isDouble() && ! value.isInt() && ! value.isInt64())
                return false;

            destination = static_cast<double> (value);
            return true;
        }
    }

    juce::File TargetLibrary::getUserTargetsFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("QC App")
                   .getChildFile ("targets.json");
    }

    juce::String TargetLibrary::getBuiltInJson()
    {
        return juce::String::fromUTF8 (BinaryData::targets_json, BinaryData::targets_jsonSize);
    }

    TargetLoadResult TargetLibrary::parse (const juce::String& jsonText,
                                           const juce::String& sourceDescription)
    {
        TargetLoadResult result;
        result.sourceDescription = sourceDescription;

        juce::var parsed;
        const auto parseResult = juce::JSON::parse (jsonText, parsed);

        if (parseResult.failed())
        {
            result.problems.add ("Could not parse the target definitions: " + parseResult.getErrorMessage());
            return result;
        }

        const auto entries = parsed["targets"];

        if (! entries.isArray())
        {
            result.problems.add ("The target definitions have no \"targets\" array.");
            return result;
        }

        for (int i = 0; i < entries.size(); ++i)
        {
            const auto& entry = entries[i];
            const auto label = "Entry " + juce::String (i + 1);

            Target target;
            target.id = entry["id"].toString().toStdString();
            target.name = entry["name"].toString().toStdString();

            if (target.id.empty() || target.name.empty())
            {
                result.problems.add (label + " is missing an id or a name, and was skipped.");
                continue;
            }

            if (! readDouble (entry, "integratedLufs", target.integratedLufs))
            {
                result.problems.add (juce::String (target.name)
                                     + " has no integratedLufs value, and was skipped.");
                continue;
            }

            if (! readDouble (entry, "maxTruePeakDb", target.maxTruePeakDb))
            {
                result.problems.add (juce::String (target.name)
                                     + " has no maxTruePeakDb value, and was skipped.");
                continue;
            }

            double tolerance = 0.0;
            if (readDouble (entry, "toleranceLu", tolerance))
            {
                if (tolerance < 0.0)
                {
                    result.problems.add (juce::String (target.name)
                                         + " has a negative tolerance, and was skipped.");
                    continue;
                }

                target.toleranceLu = tolerance;
            }

            double maxLra = 0.0;
            if (readDouble (entry, "maxLoudnessRangeLu", maxLra))
                target.maxLoudnessRangeLu = maxLra;

            const auto gating = entry["gating"].toString();

            if (gating == "dialogue")
            {
                target.gating = GatingMode::dialogue;
            }
            else if (gating.isNotEmpty() && gating != "program")
            {
                result.problems.add (juce::String (target.name) + " has an unrecognised gating mode \""
                                     + gating + "\", and was skipped.");
                continue;
            }

            target.lastVerified = entry["lastVerified"].toString().toStdString();

            if (target.lastVerified.empty())
                result.unverified.add (juce::String (target.name));

            result.targets.push_back (target);
        }

        if (result.targets.empty() && result.problems.isEmpty())
            result.problems.add ("The target definitions contain no usable entries.");

        return result;
    }

    TargetLoadResult TargetLibrary::load()
    {
        const auto userFile = getUserTargetsFile();

        if (userFile.existsAsFile())
            return parse (userFile.loadFileAsString(), userFile.getFullPathName());

        // First run: hand the user an editable copy. A failure here is not fatal - the
        // built-in definitions still work - but it is worth saying out loud, because the
        // user would otherwise edit a file the app never reads.
        auto result = parse (getBuiltInJson(), "built-in defaults");

        if (userFile.getParentDirectory().createDirectory())
        {
            if (! userFile.replaceWithText (getBuiltInJson()))
                result.problems.add ("Could not write an editable copy of the target definitions to "
                                     + userFile.getFullPathName());
        }
        else
        {
            result.problems.add ("Could not create " + userFile.getParentDirectory().getFullPathName());
        }

        return result;
    }
}
