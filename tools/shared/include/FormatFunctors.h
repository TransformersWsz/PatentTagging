//
// Created by yuan on 7/31/19.
//

#pragma once
#ifndef TOOLS_FORMATFUNCTORS_H
#define TOOLS_FORMATFUNCTORS_H

#include <string>
#include <vector>

#include "FunctorDict.h"
#include "Utility.h"


struct TagTextFormatterFunctor : public Cloneable {
    virtual std::string operator()(const std::string& text) = 0;
    DECLARE_ABSTRACT_CLONE(TagTextFormatterFunctor);
};


struct ClassificationFormatter : public TagTextFormatterFunctor {
    std::string operator()(const std::string& classification) override
    {
        return ReplaceDelimiter<'-'>(std::string(classification));
    }

    DEFINE_DEFAULT_CLONE(ClassificationFormatter);
};


class AbstractSeparateFormatter : public TagTextFormatterFunctor {
    SplitParagraph splitParagraph;
public:
    std::string operator()(const std::string& abstract) override
    {
        return splitParagraph(abstract, '\n');
    }

    DEFINE_DEFAULT_CLONE(AbstractSeparateFormatter);
};


/* A mapped functor storing customized function to format inner text of based on a given tag */
using TagTextFormatterDict = FunctorDict<TagTextFormatterFunctor, std::string, const std::string&>;


struct TagTextOutputFormatter : public Cloneable {
    virtual std::string operator()(const TagTextDict& tagTextDict) = 0;
    DECLARE_ABSTRACT_CLONE(TagTextOutputFormatter);

    virtual ~TagTextOutputFormatter() = default;
};


class IdClassAbstractFileOutput : public TagTextOutputFormatter {
    TruncateUnicodeString truncateUnicodeString_;
public:
    std::string operator()(const TagTextDict& tagTextDict) override;

    DEFINE_DEFAULT_CLONE(IdClassAbstractFileOutput);
};

class IdSmallClassAbstractFileOutput : public TagTextOutputFormatter {
    TruncateUnicodeString truncateUnicodeString_;
public:
    std::string operator()(const TagTextDict& tagTextDict) override;

    DEFINE_DEFAULT_CLONE(IdSmallClassAbstractFileOutput);
};


/* split abstract text into sentences separated by new lines
 * may throw std::range_error when encountering non-unicode
 * characters */
class SplitAbstractFileOutput : public TagTextOutputFormatter {
    SplitParagraph splitParagraph_;
public:
    std::string operator()(const TagTextDict& tagTextDict) override;

    DEFINE_DEFAULT_CLONE(SplitAbstractFileOutput);
};


class FirstClassOutput : public TagTextOutputFormatter {
public:
    std::string operator()(const TagTextDict& tagTextDict) override;

    DEFINE_DEFAULT_CLONE(FirstClassOutput);
};


class FirstClassFirstSpaceOutput : public TagTextOutputFormatter {
public:
    std::string operator()(const TagTextDict& tagTextDict) override;

    DEFINE_DEFAULT_CLONE(FirstClassFirstSpaceOutput);
};


using TagTextOutputFormatterDict = FunctorDict<TagTextOutputFormatter, std::string, const TagTextDict&>;


#endif //TOOLS_FORMATFUNCTORS_H
