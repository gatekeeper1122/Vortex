export interface IReference {
    fileMD5?: string;
    modId?: string;
    versionMatch?: string;
    logicalFileName?: string;
    fileExpression?: string;
}
export declare type RuleType = 'before' | 'after' | 'requires' | 'conflics' | 'recommends' | 'provides';
export interface IRule {
    type: RuleType;
    reference: IReference;
}
export interface IModInfo {
    modId: string;
    modName: string;
    fileName: string;
    fileSizeBytes: number;
    gameId: string;
    logicalFileName?: string;
    fileVersion: string;
    fileMD5: string;
    sourceURI: any;
    rules?: IRule[];
    details?: {
        homepage?: string;
        category?: string;
        description?: string;
        author?: string;
    };
}
export interface ILookupResult {
    key: string;
    value: IModInfo;
}
export interface IHashResult {
    md5sum: string;
    numBytes: number;
}